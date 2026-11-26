#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Omm.h"

namespace
{
using sgp4sdp4::tle_t;
using sgp4sdp4::vector_t;
using namespace std;

const string header =
    "OBJECT_NAME,NORAD_CAT_ID,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,"
    "RA_OF_ASC_NODE,ARG_OF_PERICENTER,MEAN_ANOMALY,BSTAR,MEAN_MOTION_DOT,MEAN_MOTION_DDOT";

void
require(bool ok, const char *message)
{
    if (!ok)
    {
        cerr << message << '\n';
        exit(1);
    }
}

bool
parse(const string &csv, vector<omm::Record> &records, vector<omm::Diagnostic> &diagnostics)
{
    istringstream input(csv);
    return omm::readCsv(input, records, diagnostics);
}

string
orbit(const string &id, const string &epoch,
      const string &fields = "15.66405360,0.0006431,51.6297,78.4979,344.6528,119.5204,0.00019085,0.00019728,0")
{
    return "\"Long object name, with \"\"a quote\"\"\"," + id + ',' + epoch + ',' + fields;
}

vector_t
propagate(tle_t elements, double minutes, int expectedDeep = -1)
{
    sgp4sdp4::ClearFlag(ALL_FLAGS);
    sgp4sdp4::select_ephemeris(&elements);
    const bool deep = sgp4sdp4::isFlagSet(DEEP_SPACE_EPHEM_FLAG) != 0;
    require(expectedDeep < 0 || deep == (expectedDeep == 1), "Wrong propagation model selected");
    vector_t position = {}, velocity = {};
    if (deep) sgp4sdp4::SDP4(minutes, &elements, &position, &velocity);
    else sgp4sdp4::SGP4(minutes, &elements, &position, &velocity);
    sgp4sdp4::Convert_Sat_State(&position, &velocity);
    require(std::isfinite(position.w) && position.w > xkmper,
            "Orbit should propagate to a finite position above Earth");
    return position;
}

void
checkAgainstTle(char lines[3][80], const tle_t &elements, bool deep)
{
    tle_t legacy = {};
    require(sgp4sdp4::Get_Next_Tle_Set(lines, &legacy) == 1, "Invalid TLE test fixture");
    require(legacy.catnr == elements.catnr && legacy.epoch_julian == 0,
            "Catalog number or legacy epoch mode changed");
    const double julian = sgp4sdp4::Julian_Date_of_Epoch(legacy.epoch);
    require(abs(elements.epoch_julian - julian) < 1e-9, "Mismatch in epoch conversion");
    for (const double minutes : {-120.0, 0.0, 55.0, 830.0})
    {
        const auto tv = propagate(legacy, minutes, deep);
        const auto ov = propagate(elements, minutes + (julian - elements.epoch_julian) * xmnpda, deep);
        // Single Julian doubles resolve roughly 40 us near these dates.
        const double diff = std::hypot(std::hypot(ov.x - tv.x, ov.y - tv.y), ov.z - tv.z);
        require(diff < 0.001, "TLE and OMM orbits differ by at least a metre");
    }
}
}

int
main(int argc, char **argv)
{
    vector<omm::Record> records;
    vector<omm::Diagnostic> diagnostics;

    if (argc == 2)
    {
        // Optional local check of a saved provider response. CI does not download data.
        ifstream input(argv[1]);
        require(omm::readCsv(input, records, diagnostics), "Could not read provider sample");
        for (const auto &record : records)
        {
            const auto state = propagate(record.elements, 0);
            cout << record.elements.catnr << '\t' << record.name << '\t' << state.w << " km\n";
        }
        return 0;
    }

    const string iss = orbit("25544", "2004-01-05T12:28:09.630624Z");
    require(parse("\xEF\xBB\xBF" + header + "\r\n" + iss, records, diagnostics),
            "BOM, CRLF or no terminal newline was not accepted");
    require(records.size() == 1 && records[0].name == "Long object name, with \"a quote\"",
            "CSV quoting or long object name was not preserved");
    char tle[3][80] = {
        "ISS (ZARYA)",
        "1 25544U 98067A   04005.51955591  .00019728  00000-0  19085-3 0  9701",
        "2 25544  51.6297  78.4979 0006431 344.6528 119.5204 15.66405360292643"
    };
    checkAgainstTle(tle, records.front().elements, false);

    const string deep = orbit("100000", "2024-02-29T00:00:00",
                              "1.00270000,0.0001200,3.4000,30.2000,119.1000,225.3000,0,0,0");
    require(parse(header + "\n" + deep + "\n", records, diagnostics),
            "Deep-space OMM fixture was not accepted");
    char alphaFive[3][80] = {
        "Synthetic GEO",
        "1 A0000U 14001A   24060.00000000  .00000000  00000-0  00000-0 0  9998",
        "2 A0000   3.4000  30.2000 0001200 119.1000 225.3000  1.00270000    12"
    };
    checkAgainstTle(alphaFive, records.front().elements, true);

    const string geo = orbit("999999999", "2100-03-01T00:00:00.125",
                             "1.0027000011,0,0,30.25,119.192,225.30123,0,-1e-6,2e-9");
    const string implied = ",CENTER_NAME,REF_FRAME,TIME_SYSTEM,MEAN_ELEMENT_THEORY,EPHEMERIS_TYPE,OBJECT_ID";
    require(parse(header + implied + "\n" + geo + ",EARTH,TEME,UTC,SGP4,0,\n",
                  records, diagnostics), "Explicit metadata, nine-digit ID or blank final value failed");
    const auto &element = records.front().elements;
    require(element.catnr == 999999999, "Nine-digit ID lost precision");
    require(element.xno == 1.0027000011 && element.xmo == 225.30123 &&
            element.xndt2o == -1e-6 && element.xndd6o == 2e-9, "Orbital element lost precision");
    const double epoch2100 = 2488128.5;
    require(abs((element.epoch_julian - epoch2100) * secday - .125) < .0001,
            "Full UTC epoch or fractional seconds lost");
    propagate(element, 36, true);

    const string leo = orbit("100001", "2026-11-25T00:00:00",
                             "15.1,0,180,30.25,119.192,225.30123,0,0,0");
    require(parse(header + "\n" + leo + '\n', records, diagnostics),
            "Circular retrograde OMM fixture was not accepted");
    propagate(records.front().elements, 36, false);

    const auto expectBadRow = [&](const string &row, const string &keyword)
    {
        require(!parse(header + '\n' + row + '\n' + iss + '\n', records, diagnostics),
                "Malformed row accepted");
        require(records.size() == 1 && records[0].elements.catnr == 25544 &&
                diagnostics.size() == 1 && diagnostics[0].line == 2 &&
                diagnostics[0].message.find(keyword) != string::npos,
                "Malformed row did not produce the expected diagnostic or blocked the next row");
    };
    expectBadRow(orbit("1000000000", "2024-02-29T00:00:00"), "NORAD_CAT_ID");
    expectBadRow(orbit("0", "2024-02-29T00:00:00"), "NORAD_CAT_ID");
    expectBadRow(orbit("Q12345", "2024-02-29T00:00:00"), "NORAD_CAT_ID");
    expectBadRow(orbit("100001", "2100-02-29T00:00:00"), "EPOCH");
    expectBadRow(orbit("100001", "2024-02-29T00:00:00+00:00"), "EPOCH");
    expectBadRow(orbit("100001", "2024-02-29T00:00:00",
                       "NaN,0,0,30.25,119.192,225.30123,0,0,0"), "MEAN_MOTION");
    expectBadRow(orbit("100001", "2024-02-29T00:00:00",
                       "1e-50,0,0,30.25,119.192,225.30123,0,0,0"), "MEAN_MOTION");
    expectBadRow(orbit("100001", "2024-02-29T00:00:00",
                       "100,0,0,30.25,119.192,225.30123,0,0,0"), "MEAN_MOTION");
    expectBadRow(orbit("100001", "2024-02-29T00:00:00",
                       "15.1,0.5,0,30.25,119.192,225.30123,0,0,0"), "perigee");
    expectBadRow(string(9000, 'x'), "too long");
    expectBadRow("\"an unterminated quote", "Malformed");

    for (const auto &bad : {"MARS,TEME,UTC,SGP4,0,", "EARTH,J2000,UTC,SGP4,0,",
                           "EARTH,TEME,TAI,SGP4,0,", "EARTH,TEME,UTC,SGP4-XP,4,",
                           "EARTH,TEME,UTC,SGP4,4,"})
        require(!parse(header + implied + "\n" + geo + ',' + bad + '\n', records, diagnostics)
                && records.empty() && diagnostics.size() == 1 && diagnostics.front().line == 2,
                "Unsupported metadata accepted");

    require(!parse("NORAD_CAT_ID,NORAD_CAT_ID\n25544,25544\n", records, diagnostics),
            "Duplicate column accepted");
    require(!parse("NORAD_CAT_ID,EPOCH\n25544,2004-01-05T12:28:09\n", records, diagnostics),
            "Missing orbital columns accepted");
    require(!parse(header + "\n", records, diagnostics), "Header-only CSV accepted");
    require(!parse(header + '\n' + iss + '\n' + orbit("25544", "2004-01-06T12:28:09") + '\n',
                   records, diagnostics) && records.size() == 1 && diagnostics.size() == 1 &&
            diagnostics[0].line == 3 &&
            diagnostics[0].message.find("Duplicate NORAD_CAT_ID") != string::npos &&
            records[0].elements.epoch_julian < 2453011,
            "Duplicate ID should keep the first record and identify the duplicate line");
    cout << "OMM parsing and both propagation models passed\n";
}

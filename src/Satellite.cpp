#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
using namespace std;

#include <sys/time.h>

#include "xpUtil.h"

#include "Satellite.h"

using namespace sgp4sdp4;

Satellite::Satellite(char tle_lines[3][80])
{
    checkLocale(LC_NUMERIC, "C");
    good = (Get_Next_Tle_Set(tle_lines, &tle) == 1);
    checkLocale(LC_NUMERIC, "");
    memcpy(tle_entry, tle_lines, 240);
}

Satellite::Satellite(const tle_t &elements, const std::string &name)
    : good(true), ommElements(elements), ommName(name), tle(elements)
{
}

Satellite::~Satellite()
{
}

int
Satellite::getID() const
{
    return(tle.catnr);
}

const char * 
Satellite::getName() const
{
    return ommName.empty() ? tle.sat_name : ommName.c_str();
}

bool
Satellite::isGoodData() const
{
    return(good);
}

void
Satellite::loadTLE()
{
    if (ommElements.epoch_julian > 0)
    {
        tle = ommElements;
    }
    else
    {
        checkLocale(LC_NUMERIC, "C");
        Get_Next_Tle_Set(tle_entry, &tle);
        checkLocale(LC_NUMERIC, "");
    }
    ClearFlag(ALL_FLAGS);
    select_ephemeris(&tle);  // SGP4 or SDP4 depending on period
}

bool
Satellite::getSpherical(const time_t tv_sec, double &lat, double &lon,
                        double &alt)
{
    const tm *utc = gmtime(&tv_sec);
    if (utc == NULL) return false;
    const double jul_utc = toJulian(utc->tm_year + 1900, utc->tm_mon + 1,
                                   utc->tm_mday, utc->tm_hour, utc->tm_min,
                                   utc->tm_sec);

    double jul_epoch = tle.epoch_julian;
    if (jul_epoch == 0)
    {
        double year;
        const double day = modf(tle.epoch * 1E-3, &year) * 1E3;
        year += year < 57 ? 2000 : 1900;  // TLE pivot: 1957 through 2056.
        jul_epoch = toJulian(static_cast<int>(year), 1, 0, 0, 0, 0) + day;
    }

    /* Zero vector for initializations */
    vector_t zero_vector = {0,0,0,0};
    vector_t pos = zero_vector;
    vector_t vel = zero_vector;

    double tsince = (jul_utc - jul_epoch) * xmnpda;

    if( isFlagSet(DEEP_SPACE_EPHEM_FLAG) )
        SDP4(tsince, &tle, &pos, &vel);
    else
        SGP4(tsince, &tle, &pos, &vel);
    
    /* Scale position and velocity vectors to km and km/sec */
    Convert_Sat_State( &pos, &vel );
    if (tle.epoch_julian > 0 &&
        (!std::isfinite(pos.w) || !std::isfinite(pos.x) || !std::isfinite(pos.y) ||
         !std::isfinite(pos.z) || pos.w < 6350))
        return false;

    /* Satellite's predicted geodetic position */
    geodetic_t sat_geodetic;

    /** All angles in rads. Distance in km. Velocity in km/s **/
    /* Calculate satellite Lat North, Lon East and Alt. */
    Calculate_LatLonAlt(jul_utc, &pos, &sat_geodetic);

    lat = sat_geodetic.lat;
    lon = sat_geodetic.lon;
    alt = sat_geodetic.alt;

    // return alt in units of earth radii
    alt = alt/6378.14 + 1;

    return tle.epoch_julian == 0 ||
           (std::isfinite(lat) && std::isfinite(lon) && std::isfinite(alt) && alt >= 1);
}

void
Satellite::printTLE() const
{
    cout << "epoch = " << tle.epoch << endl;
    cout << "xndt2o = " << tle.xndt2o << endl;

    cout << "xndd6o = " << tle.xndd6o << endl;
    cout << "bstar = " << tle.bstar << endl;
    cout << "xincl = " << tle.xincl << endl;
    cout << "xnodeo = " << tle.xnodeo << endl;
    cout << "eo = " << tle.eo << endl;
    cout << "omegao = " << tle.omegao << endl;
    cout << "xmo = " << tle.xmo << endl;
    cout << "xno = " << tle.xno << endl;
    cout << "catnr = " << tle.catnr << endl;
    cout << "elset = " << tle.elset << endl;
    cout << "revnum = " << tle.revnum << endl;
    cout << "name = " << tle.sat_name << endl;
    cout << "idesg = " << tle.idesg << endl;
    cout << endl;
}

bool
Satellite::operator == (const Satellite &sat) const
{
    return(tle.catnr == sat.tle.catnr);
}

bool
parseSatelliteCatalogId(const char *text, int &id)
{
    if (text == NULL) return false;
    if (*text == '+') ++text;  // Also accepted by legacy satellite annotation rows.
    int value = 0;
    for (int digits = 0; *text != '\0'; ++text, ++digits)
    {
        if (digits >= 9 || *text < '0' || *text > '9') return false;
        value = value * 10 + (*text - '0');
    }
    if (value == 0) return false;
    id = value;
    return true;
}

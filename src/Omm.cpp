#include "Omm.h"

#include <cmath>
#include <istream>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace
{
using std::string;
using std::vector;

using Columns = std::map<string, std::size_t>;

string
trim(const string &text)
{
    const auto first = text.find_first_not_of(" \t\r");
    if (first == string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

bool
splitCsv(const string &line, vector<string> &fields)
{
    if (line.find('\0') != string::npos) return false;
    std::size_t pos = 0;
    do
    {
        string field;
        if (pos < line.size() && line[pos] == '"')
        {
            ++pos;
            bool closed = false;
            while (pos < line.size())
            {
                const char c = line[pos++];
                if (c != '"') field += c;
                else if (pos < line.size() && line[pos] == '"')
                {
                    ++pos;
                    field += '"';
                }
                else
                {
                    closed = true;
                    break;
                }
            }
            if (!closed || (pos < line.size() && line[pos] != ',')) return false;
        }
        else
        {
            const auto end = line.find(',', pos);
            field = line.substr(pos, end == string::npos ? end : end - pos);
            if (field.find('"') != string::npos) return false;
            pos = end == string::npos ? line.size() : end;
        }
        fields.push_back(trim(field));
    } while (pos < line.size() && line[pos++] == ',');
    return true;
}

const string &
value(const Columns &columns, const vector<string> &fields, const char *key)
{
    static const string empty;
    const auto it = columns.find(key);
    return it == columns.end() ? empty : fields[it->second];
}

bool
decimal(const string &text, std::size_t pos, std::size_t count, int &n)
{
    if (count == 0 || count > 9 || pos > text.size() || count > text.size() - pos)
        return false;
    n = 0;
    for (std::size_t i = pos; i < pos + count; ++i)
    {
        if (text[i] < '0' || text[i] > '9') return false;
        n = n * 10 + (text[i] - '0');
    }
    return true;
}

bool
readNumber(const string &text, double &number)
{
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    return (in >> number) && in.eof() && std::isfinite(number);
}

bool
readEpoch(const string &text, double &epochJulian)
{
    int year, month, day, hour, minute, second;
    if (text.size() < 19 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' ||
        !decimal(text, 0, 4, year) || !decimal(text, 5, 2, month) ||
        !decimal(text, 8, 2, day) || !decimal(text, 11, 2, hour) ||
        !decimal(text, 14, 2, minute) || !decimal(text, 17, 2, second) ||
        year < 1900 || month < 1 || month > 12 || hour > 23 ||
        minute > 59 || second > 59) return false;

    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int days[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (day < 1 || day > days[month - 1]) return false;

    const std::size_t end = text.size() - (text.back() == 'Z' ? 1 : 0);
    double fraction = 0;
    if (end != 19)
    {
        if (end < 21 || text[19] != '.') return false;
        double place = 0.1;
        for (std::size_t i = 20; i < end; ++i)
        {
            if (text[i] < '0' || text[i] > '9') return false;
            fraction += (text[i] - '0') * place;
            place *= 0.1;
        }
    }

    int dayOfYear = day;
    for (int m = 1; m < month; ++m) dayOfYear += days[m - 1];
    epochJulian = sgp4sdp4::Julian_Date_of_Year(year) + dayOfYear
                  + (hour * 3600 + minute * 60 + second + fraction) / 86400;
    return true;
}

struct NumberField
{
    const char *name;
    double sgp4sdp4::tle_t::*member;
};

const NumberField numberFields[] =
{
    {"MEAN_MOTION", &sgp4sdp4::tle_t::xno},
    {"ECCENTRICITY", &sgp4sdp4::tle_t::eo},
    {"INCLINATION", &sgp4sdp4::tle_t::xincl},
    {"RA_OF_ASC_NODE", &sgp4sdp4::tle_t::xnodeo},
    {"ARG_OF_PERICENTER", &sgp4sdp4::tle_t::omegao},
    {"MEAN_ANOMALY", &sgp4sdp4::tle_t::xmo},
    {"BSTAR", &sgp4sdp4::tle_t::bstar},
    {"MEAN_MOTION_DOT", &sgp4sdp4::tle_t::xndt2o},
    {"MEAN_MOTION_DDOT", &sgp4sdp4::tle_t::xndd6o}
};

string
readRecord(const Columns &columns, const vector<string> &fields, omm::Record &record)
{
    auto &elements = record.elements;
    const auto &id = value(columns, fields, "NORAD_CAT_ID");
    if (!decimal(id, 0, id.size(), elements.catnr) || elements.catnr < 1)
        return "NORAD_CAT_ID must be a positive decimal ID of at most nine digits";
    if (!readEpoch(value(columns, fields, "EPOCH"), elements.epoch_julian))
        return "EPOCH must be a valid UTC calendar timestamp (YYYY-MM-DDTHH:MM:SS[.fraction][Z])";

    // CelesTrak omits required OMM metadata when the value is implied by
    // its GP service. If present, reject fields this propagator cannot use.
    const struct { const char *name; const char *expected; } metadata[] =
    {
        {"CENTER_NAME", "EARTH"},
        {"REF_FRAME", "TEME"},
        {"TIME_SYSTEM", "UTC"},
        {"MEAN_ELEMENT_THEORY", "SGP4"}
    };
    for (const auto &item : metadata)
    {
        const auto &text = value(columns, fields, item.name);
        if (!text.empty() && text != item.expected)
            return string(item.name) + " must be " + item.expected;
    }
    const auto &type = value(columns, fields, "EPHEMERIS_TYPE");
    if (!type.empty() && type != "0" && type != "2")
        return "EPHEMERIS_TYPE must be 0 (automatic) or 2 (SGP4)";

    for (const auto &field : numberFields)
    {
        if (!readNumber(value(columns, fields, field.name), elements.*field.member))
            return string(field.name) + " must be a finite number";
    }

    // Mean motion is in rev/day, and angles in degrees. Pass the same
    // quantities the CelesTrak SGP4/TLE reader expects, without rounding
    // to the TLE text widths. select_ephemeris() takes care of units.
    // This very broad envelope includes near-decay through deep-space
    // Earth orbits; reject periods over 27 years or under 72 minutes.
    if (elements.xno < 0.0001 || elements.xno > 20)
        return "MEAN_MOTION must be between 0.0001 and 20 revolutions per day";
    if (elements.eo < 0 || elements.eo >= 1) return "ECCENTRICITY must be in [0, 1)";
    const double meanMotionRadiansPerMinute = elements.xno * twopi / xmnpda;
    const double perigeeEarthRadii = std::pow(xke / meanMotionRadiansPerMinute, 2.0 / 3)
                                   * (1 - elements.eo);
    // Give near-decay mean elements some room for the J2 correction, but
    // don't ask the propagator to calculate an orbit through solid Earth.
    if (perigeeEarthRadii < 0.99) return "Mean elements put perigee below Earth's surface";
    if (elements.xincl < 0 || elements.xincl > 180) return "INCLINATION must be in [0, 180]";
    const auto isAngle = [](double x) { return x >= 0 && x <= 360; };
    if (!isAngle(elements.xnodeo)) return "RA_OF_ASC_NODE must be in [0, 360]";
    if (!isAngle(elements.omegao)) return "ARG_OF_PERICENTER must be in [0, 360]";
    if (!isAngle(elements.xmo)) return "MEAN_ANOMALY must be in [0, 360]";

    record.name = value(columns, fields, "OBJECT_NAME");
    if (record.name.empty()) record.name = id;
    return {};
}
}

bool
omm::readCsv(std::istream &input, vector<Record> &records, vector<Diagnostic> &diagnostics)
{
    records.clear();
    diagnostics.clear();
    Columns columns;
    std::set<int> ids;
    std::size_t number = 0;
    // GP rows are generally under 250 bytes. Bound individual records, but
    // allow many rows for a full catalog download. Preserve line numbers.
    char buffer[8192];
    for (;;)
    {
        input.getline(buffer, sizeof(buffer));
        const auto length = input.gcount();
        if (input.bad() || (input.fail() && !input.eof() && length == 0))
        {
            diagnostics.push_back({number + 1, "Could not read OMM CSV"});
            break;
        }
        if (length == 0 && input.eof()) break;
        ++number;
        if (input.fail() && !input.eof())
        {
            diagnostics.push_back({number, "OMM CSV line is too long"});
            if (columns.empty()) return false;
            input.clear();
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        string line(buffer, static_cast<std::size_t>(length - (input.eof() ? 0 : 1)));
        if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (trim(line).empty()) continue;

        vector<string> fields;
        if (!splitCsv(line, fields))
        {
            diagnostics.push_back({number, "Malformed OMM CSV record"});
            if (columns.empty()) return false;
            continue;
        }
        if (columns.empty())
        {
            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                if (fields[i].empty() || !columns.emplace(fields[i], i).second)
                {
                    diagnostics.push_back({number, "Empty or duplicate OMM CSV column name"});
                    return false;
                }
            }
            const auto require = [&](const char *key)
            {
                if (columns.count(key) == 0)
                    diagnostics.push_back({number, string("Missing OMM CSV column ") + key});
            };
            require("NORAD_CAT_ID");
            require("EPOCH");
            for (const auto &field : numberFields) require(field.name);
            if (!diagnostics.empty()) return false;
            continue;
        }
        if (fields.size() != columns.size())
        {
            diagnostics.push_back({number, "OMM CSV row does not match the header"});
            continue;
        }
        Record record = {};
        const string error = readRecord(columns, fields, record);
        if (!error.empty()) diagnostics.push_back({number, error});
        else if (!ids.insert(record.elements.catnr).second)
            diagnostics.push_back({number, "Duplicate NORAD_CAT_ID; keeping the first valid row"});
        else records.push_back(record);
    }
    if (records.empty() && diagnostics.empty())
        diagnostics.push_back({number == 0 ? 1 : number, "No OMM orbital records found"});
    return diagnostics.empty();
}

#include <cmath>
#include <limits>
#include <locale>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace std;

#include "findFile.h"
#include "readOriginFile.h"
#include "SceneValidation.h"
#include "xpUtil.h"

static double
interpolateCircular(double x0, double x1, 
                    const double xMax, const double interpFactor)
{
    const double larger = (x0 > x1 ? x0 : x1);
    const double smaller = (x0 < x1 ? x0 : x1);

    if ((larger - smaller) > (smaller - larger + xMax))
    {
        if (x0 < x1)
            x0 += xMax;
        else
            x1 += xMax;
    }

    double returnVal = interpFactor * (x1 - x0) + x0;
    if (returnVal > xMax)
        returnVal -= xMax;

    return(returnVal);
}

static bool
parseOriginRecord(const string &line, LBRPoint &point, bool requirePosition)
{
    istringstream fields(line.substr(0, line.find('#')));
    fields.imbue(locale::classic());
    string date;
    if (!(fields >> date) || !parseRenderDate(date, point.time)) return false;
    point.radius = point.latitude = point.longitude = 0;
    point.localTime = -1;
    fields >> ws;
    if (fields.eof()) return !requirePosition;

    double radius, latitude, longitude;
    if (!(fields >> radius >> latitude >> longitude) || !isfinite(radius)
        || !isfinite(latitude) || !isfinite(longitude)
        || radius <= 0 || latitude < -90 || latitude > 90)
        return false;
    fields >> ws;
    if (!fields.eof())
    {
        if (!(fields >> point.localTime) || !isfinite(point.localTime)
            || point.localTime < 0 || point.localTime > 24)
            return false;
        fields >> ws;
        if (!fields.eof()) return false;
    }
    point.radius = radius;
    point.latitude = latitude * deg_to_rad;
    point.longitude = longitude * deg_to_rad;
    return true;
}

static void
readOriginRecords(string filename, vector<LBRPoint> &points,
                  bool increasingTime, bool requirePosition)
{
    if (!findFile(filename, "origin"))
        xpExit("Can't open origin file " + filename + "\n", __FILE__, __LINE__);
    ifstream file(filename.c_str());

    char line[4096];
    size_t lineNumber = 0;
    for (;;)
    {
        ++lineNumber;
        const SceneValidation::Location location(filename, lineNumber);
        if (!file.getline(line, sizeof(line)))
        {
            if (file.eof() || file.bad() || file.gcount() == 0) break;
            file.clear();
            file.ignore(numeric_limits<streamsize>::max(), '\n');
            ostringstream message;
            message << filename << ':' << lineNumber << ": origin line is too long\n";
            xpWarn(message.str(), __FILE__, __LINE__);
            continue;
        }

        const string content(line);
        const size_t start = content.find_first_not_of(" \t\r");
        if (start == string::npos || content[start] == '#') continue;
        LBRPoint point;
        if (!parseOriginRecord(content, point, requirePosition)
            || (increasingTime && !points.empty() && point.time <= points.back().time))
        {
            ostringstream message;
            message << filename << ':' << lineNumber << ": ignoring invalid origin record\n";
            xpWarn(message.str(), __FILE__, __LINE__);
            continue;
        }
        points.push_back(point);
    }

    if (points.empty())
        xpExit("No usable records in origin file " + filename + "\n", __FILE__, __LINE__);
}

void
readOriginFile(string filename, vector<LBRPoint> &originVector, bool interpolate)
{
    readOriginRecords(filename, originVector, interpolate, interpolate);
}

void
readDynamicOrigin(string filename, LBRPoint &originPoint)
{
    vector<LBRPoint> points;
    readOriginRecords(filename, points, false, true);
    originPoint = points.back();
}

void
interpolateOriginFile(const double julianDay, 
                      const vector<LBRPoint> &originVector, 
                      double &rad, 
                      double &lat, double &lon, 
                      double &localTime)
{
    const int numPoints = originVector.size();

    if (julianDay <= originVector[0].time)
    {
        rad = originVector[0].radius;
        lat = originVector[0].latitude;
        lon = originVector[0].longitude;
        localTime = originVector[0].localTime;
    }
    else if (julianDay >= originVector[numPoints-1].time)
    {
        rad = originVector[numPoints-1].radius;
        lat = originVector[numPoints-1].latitude;
        lon = originVector[numPoints-1].longitude;
        localTime = originVector[numPoints-1].localTime;
    }
    else
    {
        double interpFactor = 0;
        for (int i = 1; i < numPoints; i++)
        {
            if (julianDay < originVector[i].time)
            {
                const double jd0 = originVector[i-1].time;
                const double jd1 = originVector[i].time;

                interpFactor = (julianDay - jd0)/(jd1 - jd0);

                rad = (interpFactor 
                       * (originVector[i].radius - originVector[i-1].radius)
                       + originVector[i-1].radius);
                lat = (interpFactor 
                       * (originVector[i].latitude 
                          - originVector[i-1].latitude)
                       + originVector[i-1].latitude);
                lon = interpolateCircular(originVector[i-1].longitude,
                                          originVector[i].longitude,
                                          TWO_PI,
                                          interpFactor);
                const double local0 = originVector[i-1].localTime;
                const double local1 = originVector[i].localTime;
                localTime = (local0 < 0 || local1 < 0) ? -1
                    : interpolateCircular(local0, local1, 24, interpFactor);

                break;
            }
        }
    }
}

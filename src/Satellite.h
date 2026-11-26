#ifndef SATELLITE_H
#define SATELLITE_H

#include <ctime>
#include <string>
#include <vector>

// Load the legacy C header's system dependencies at global scope before
// placing its own declarations in a namespace (required by libstdc++).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace sgp4sdp4
{
#include "libsgp4sdp4/sgp4sdp4.h"
}

class Satellite
{
public:
    Satellite(char tle_line[3][80]);
    Satellite(const sgp4sdp4::tle_t &elements, const std::string &name,
              const std::string &source);

    ~Satellite();
    bool isGoodData() const;
    int getID() const;
    const char * getName() const;
    bool getSpherical(const time_t tv_sec, double &lat, double &lon,
                      double &alt);
    void loadTLE();

    void printTLE() const;

    bool operator == (const Satellite &sat) const;
 
private:
    bool good;
    char tle_entry[3][80] = {};
    sgp4sdp4::tle_t ommElements = {};
    std::string ommName;
    std::string ommSource;

    bool propagationFailed(const char *reason) const;

    sgp4sdp4::tle_t tle = {};
};

// Parse a decimal catalog ID accepted in satellite annotation files.
bool parseSatelliteCatalogId(const char *text, int &id);

// Load the first orbit companion by directory priority, preferring TLE only
// within a directory. Append usable orbits even if other records warn.
bool readSatelliteOrbitFile(const std::string &name, std::vector<Satellite> &satellites);

#endif

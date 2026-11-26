#ifndef READ_ORIGIN_FILE_H
#define READ_ORIGIN_FILE_H

#include <vector>
#include <string>

struct LBRPoint
{
    double time;
    double radius;
    double latitude;
    double longitude;
    double localTime;
};

extern void
readOriginFile(std::string filename, std::vector<LBRPoint> &originMap, bool interpolate);

extern void
readDynamicOrigin(std::string filename, LBRPoint &originPoint);

extern void interpolateOriginFile(const double julianDay,
                                  const std::vector<LBRPoint> &originVector,
                                  double &rad,
                                  double &lat, double &lon,
                                  double &localTime);
#endif

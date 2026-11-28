#ifndef ORBIT_MATH_H
#define ORBIT_MATH_H

#include <cmath>
#include <locale>
#include <sstream>

inline double
normalizeVector(double vector[3])
{
    const double length = std::hypot(std::hypot(vector[0], vector[1]), vector[2]);
    if (length > 0 && std::isfinite(length))
        for (int i = 0; i < 3; ++i)
            vector[i] /= length;
    return length;
}

inline double
reflectedCoordinate(double center, double point)
{
    return center - (point - center);
}

inline double
ellipticAnomaly(double eccentricity, double mean)
{
    if (!std::isfinite(mean) || !std::isfinite(eccentricity)
        || eccentricity <= 0 || eccentricity >= 1)
        return mean;

    // The equation is monotone for elliptical orbits. Keep Newton steps
    // inside a bracket that always contains the root.
    double low = mean - eccentricity, high = mean + eccentricity;
    double value = mean;
    for (int i = 0; i < 80; ++i)
    {
        const double error = value - eccentricity * std::sin(value) - mean;
        if (error == 0) return value;
        if (error > 0) high = value;
        else low = value;
        if (high - low <= 1e-10) return (low + high) / 2;

        const double slope = 1 - eccentricity * std::cos(value);
        const double next = value - error / slope;
        value = next > low && next < high ? next : (low + high) / 2;
    }
    return value;
}

template <typename T>
inline bool
parseTrailWindow(const char *text, T limit, T smallestStep, T &start, T &end, T &step)
{
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    T first, last, interval;
    char comma1, comma2;
    if (!(input >> first >> comma1 >> last >> comma2 >> interval)
        || comma1 != ',' || comma2 != ',' || !std::isfinite(first)
        || !std::isfinite(last) || !std::isfinite(interval)
        || first < -limit || last > limit || first > last || interval <= 0
        || interval > limit)
        return false;
    input >> std::ws;
    if (!input.eof()) return false;
    if (interval < smallestStep) interval = smallestStep;
    if ((static_cast<double>(last) - first) / interval > 100000) return false;
    start = first;
    end = last;
    step = interval;
    return true;
}

#endif

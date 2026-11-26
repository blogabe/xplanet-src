#include <cstdlib>
#include <iostream>

#include "OrbitMath.h"

int
main()
{
    double vector[] = { 3, 4, 12 };
    if (normalizeVector(vector) != 13
        || std::abs(std::hypot(std::hypot(vector[0], vector[1]), vector[2]) - 1) > 1e-12)
        return EXIT_FAILURE;
    const double primary[] = { 2, 5, -4 };
    const double above[] = { 10, -3, 15 };
    for (int i = 0; i < 3; ++i)
        if ((above[i] + reflectedCoordinate(primary[i], above[i])) / 2 != primary[i])
            return EXIT_FAILURE;
    const double eccentricities[] = { 0, 0.01, 0.7, 0.999999 };
    const double means[] = { 0, 1e-8, 0.05, 0.7, 1.5, 4, -2.2 };
    for (size_t e = 0; e < sizeof(eccentricities)/sizeof(double); ++e)
        for (size_t m = 0; m < sizeof(means)/sizeof(double); ++m)
        {
            const double anomaly = ellipticAnomaly(eccentricities[e], means[m]);
            if (!std::isfinite(anomaly) ||
                std::abs(anomaly - eccentricities[e] * std::sin(anomaly) - means[m]) > 1e-9)
                return EXIT_FAILURE;
        }
    int first, last, every;
    if (!parseTrailWindow("-15,30,5", 1000000, 1, first, last, every)
        || first != -15 || last != 30 || every != 5
        || parseTrailWindow("-15,30", 1000000, 1, first, last, every))
        return EXIT_FAILURE;
    double start, end, step;
    if (!parseTrailWindow("-0.25, 0.5, 0.01", 36525.0, 1e-4, start, end, step)
        || start != -0.25 || end != 0.5 || step != 0.01
        || parseTrailWindow("0.5,0.0,0.1", 36525.0, 1e-4, start, end, step))
        return EXIT_FAILURE;
    std::cout << "Orbit math and trail cases passed\n";
}

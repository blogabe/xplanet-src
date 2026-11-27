#include <SpiceUsr.h>
#include <cmath>
#include <cstdio>
#include "EphemerisSpice.h"

namespace
{
constexpr double km_per_au = 149597870.7;

void segment(SpiceInt handle, SpiceInt target, SpiceInt center,
             const char *name, double x, double y, double z)
{
    const SpiceDouble states[2][6] = {
        {x, y, z, 0, 0, 0},
        {x, y, z, 0, 0, 0}
    };
    spkw08_c(handle, target, center, "J2000", -86400, 86400,
             name, 1, 2, states, -86400, 172800);
}

void write_kernel(const char *path, double direction)
{
    SpiceInt handle;
    spkopn_c(path, "Xplanet test", 0, &handle);
    segment(handle, 399, 10, "Synthetic Earth",
            direction * 0.4 * km_per_au, direction * -0.8 * km_per_au,
            direction * 0.35 * km_per_au);
    // Use the registered Voyager 1 ID, but a synthetic 10,000 km vector
    // at latitude 30 N and longitude 45 E at J2000, not a mission kernel.
    segment(handle, -31, 399, "Synthetic spacecraft", 7133.716, -4910.203, 5000);
    spkcls_c(handle);
}

bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-6;
}
}

int main()
{
    write_kernel("spice/positive.bsp", 1);
    write_kernel("spice/negative.bsp", -1);
    // Exercise the exact production conversion, including its sign and scale.
    furnsh_c("spice/positive.bsp");
    EphemerisSpice ephemeris;
    double x = 0, y = 0, z = 0;
    ephemeris.GetHeliocentricXYZ(EARTH, 2451545.0, x, y, z);
    unload_c("spice/positive.bsp");
    if (!near(x, 0.4) || !near(y, -0.8) || !near(z, 0.35))
    {
        std::fprintf(stderr, "Unexpected Earth position in AU: %g %g %g\n", x, y, z);
        return 1;
    }
    return 0;
}

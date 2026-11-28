#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "libmultiple/RayleighTable.h"

int
main()
{
    if (!validRayleighTemplate("earth%03d.png") ||
        !validRayleighTemplate("disk%d.bin") ||
        validRayleighTemplate("disk.png") ||
        validRayleighTemplate("disk%d-%d.png"))
        return EXIT_FAILURE;

    FILE *file = std::fopen("rayleigh.bin", "wb");
    const size_t dims[] = { 3, 2, 2 };
    const double values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    if (file == NULL || std::fwrite(dims, sizeof(size_t), 3, file) != 3 ||
        std::fwrite(values, sizeof(double), 12, file) != 12 || std::fclose(file) != 0)
        return EXIT_FAILURE;
    double *read = readRayleighTable("rayleigh.bin", 3, 2, 2);
    if (read == NULL) return EXIT_FAILURE;
    for (size_t i = 0; i < 12; ++i)
        if (read[i] != values[i]) return EXIT_FAILURE;
    delete [] read;

    if (readRayleighTable("not-present.bin", 3, 2, 2) != NULL ||
        readRayleighTable("rayleigh.bin", 3, 2, 3) != NULL)
        return EXIT_FAILURE;
    std::cout << "Rayleigh table cases passed\n";
}

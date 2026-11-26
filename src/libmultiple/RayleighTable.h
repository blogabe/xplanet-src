#ifndef RAYLEIGH_TABLE_H
#define RAYLEIGH_TABLE_H

#include <cstdio>
#include <new>
#include <string>

// A table path is a filename with one integer phase field, e.g. disk%03d.png.
inline bool
validRayleighTemplate(const std::string &name)
{
    bool integer = false;
    for (size_t i = 0; i < name.size(); ++i)
    {
        if (name[i] != '%') continue;
        if (++i == name.size()) return false;
        if (name[i] == '%') continue;
        if (integer) return false;
        int digits = 0;
        while (i < name.size() && name[i] >= '0' && name[i] <= '9')
        {
            if (++digits > 1 && name[i - 1] != '0') return false;
            if (digits > 2) return false;
            ++i;
        }
        if (i == name.size() || name[i] != 'd') return false;
        integer = true;
    }
    return integer;
}

// The existing table format uses native size_t dimensions and native doubles.
inline double *
readRayleighTable(const char *filename, size_t bands, size_t columns, size_t rows)
{
    const size_t maxEntries = 1024 * 1024;
    if (bands == 0 || columns == 0 || rows == 0 ||
        bands > maxEntries / columns || bands * columns > maxEntries / rows)
        return NULL;

    FILE *file = std::fopen(filename, "rb");
    if (file == NULL) return NULL;
    const size_t count = bands * columns * rows;
    size_t dims[3];
    double *values = NULL;
    if (std::fread(dims, sizeof(size_t), 3, file) == 3 &&
        dims[0] == bands && dims[1] == columns && dims[2] == rows)
    {
        values = new (std::nothrow) double[count];
        if (values != NULL && std::fread(values, sizeof(double), count, file) != count)
        {
            delete [] values;
            values = NULL;
        }
    }
    std::fclose(file);
    return values;
}

#endif

#ifndef MAP_REDUCTION_H
#define MAP_REDUCTION_H

#include <climits>
#include <cstddef>
#include <stdint.h>

// Return a new RGB map, or NULL when no reduction is needed. The minimum
// width limits the scale; it never forces the output to a 2:1 shape.
inline unsigned char *
reduceMapRGB(const unsigned char *source, int width, int height, int factor,
             int &reducedWidth, int &reducedHeight)
{
    if (source == NULL || width <= 0 || height <= 0 || factor <= 0 ||
        width > (INT_MAX / 3) / height)
        return NULL;

    int w = width;
    int h = height;
    while (factor > 0 && w / 2 >= 16)
    {
        w /= 2;
        if (h > 1) h /= 2;
        --factor;
    }
    if (w == width) return NULL;

    unsigned char *result = new unsigned char[3 * w * h];
    for (int j = 0; j < h; ++j)
    {
        const int y0 = static_cast<int>(static_cast<int64_t>(j) * height / h);
        const int y1 = static_cast<int>(static_cast<int64_t>(j + 1) * height / h);
        for (int i = 0; i < w; ++i)
        {
            // Partition the source without dropping right/bottom edge pixels.
            const int x0 = static_cast<int>(static_cast<int64_t>(i) * width / w);
            const int x1 = static_cast<int>(static_cast<int64_t>(i + 1) * width / w);
            const uint64_t count = static_cast<uint64_t>(y1 - y0) * (x1 - x0);
            uint64_t sums[3] = { 0, 0, 0 };
            for (int y = y0; y < y1; ++y)
            {
                const unsigned char *pixel = source + 3 * (y * width + x0);
                for (int x = x0; x < x1; ++x, pixel += 3)
                    for (int k = 0; k < 3; ++k)
                        sums[k] += pixel[k];
            }
            for (int k = 0; k < 3; ++k)
                result[3 * (j * w + i) + k] = static_cast<unsigned char>(sums[k] / count);
        }
    }

    reducedWidth = w;
    reducedHeight = h;
    return result;
}

#endif

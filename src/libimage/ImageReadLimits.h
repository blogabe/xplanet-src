#ifndef IMAGE_READ_LIMITS_H
#define IMAGE_READ_LIMITS_H

#include <stddef.h>
#include <stdint.h>

/* Bound decoded pixels before allocation; offsets still fit existing int users. */
static inline int
image_read_area(uint64_t width, uint64_t height, size_t *area)
{
    const uint64_t max_pixels = 64 * 1024 * 1024;
    if (width == 0 || height == 0 || width > 21600 || height > 21600
        || width > max_pixels / height)
        return 0;

    *area = (size_t) (width * height);
    return 1;
}

#endif

#ifndef IMAGE_READ_LIMITS_H
#define IMAGE_READ_LIMITS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Bound the primary decoded buffers for one image (not process-wide memory).
 * Allow 21600x10800 global maps, including for codecs with scratch rasters.
 * Keep up to four bytes per pixel addressable by existing signed-int offsets.
 */
static inline int
image_read_area(uint64_t width, uint64_t height, unsigned bytes_per_pixel,
                const char *format, size_t *area)
{
    const uint64_t max_pixels = UINT64_C(256) * 1024 * 1024;
    const uint64_t max_bytes = UINT64_C(1792) * 1024 * 1024;
    if (width == 0 || height == 0) return 0;
    if (width > 32768 || height > 32768 || width > max_pixels / height
        || width * height > max_bytes / bytes_per_pixel
        || width * height > SIZE_MAX / bytes_per_pixel)
    {
        fprintf(stderr, "%s map dimensions %llux%llu exceed the decoded image limit.\n",
                format, (unsigned long long) width, (unsigned long long) height);
        return 0;
    }

    *area = (size_t) (width * height);
    return 1;
}

#endif

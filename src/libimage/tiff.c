/****************************************************************************
    tiff.c - read and write tiff images using libtiff routines.
    Distributed with Xplanet.
    Copyright (C) 2002 Hari Nair <hari@alumni.caltech.edu>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <tiffio.h>

#include "ImageReadLimits.h"

int
read_tiff(const char *filename, int *width, int *height, unsigned char **rgb)
{
    TIFF *tif = TIFFOpen(filename, "r");
    if (tif == NULL) return 0;

    uint32_t w, h;
    size_t area;
    uint32_t *raster = NULL;
    unsigned char *pixels = NULL;
    int ok = 0;
    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w)
        || !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h)
        || !image_read_area(w, h, 7, "TIFF", &area))
        goto cleanup;

    raster = malloc(area * sizeof(*raster));
    if (raster == NULL
        || !TIFFReadRGBAImageOriented(tif, w, h, raster, ORIENTATION_TOPLEFT, 1))
        goto cleanup;

    pixels = malloc(3 * area);
    if (pixels == NULL) goto cleanup;
    for (size_t i = 0; i < area; ++i)
    {
        pixels[3*i] = TIFFGetR(raster[i]);
        pixels[3*i + 1] = TIFFGetG(raster[i]);
        pixels[3*i + 2] = TIFFGetB(raster[i]);
    }
    *width = w;
    *height = h;
    *rgb = pixels;
    ok = 1;

cleanup:
    free(raster);
    TIFFClose(tif);
    return ok;
}

int
write_tiff(FILE *file, int width, int height, unsigned char *rgb)
{
    /* Let the shared writer close the descriptor and check its status. */
    TIFF *tif = TIFFFdOpen(fileno(file), "Xplanet output", "w");
    if (tif == NULL) return 0;

    int ok = TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8)
        && TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32_t) width)
        && TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t) height)
        && TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB)
        && TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG)
        && TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3)
        && TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));
    for (int j = 0; ok && j < height; ++j)
        ok = TIFFWriteScanline(tif, rgb + j * width * 3, j, 0) == 1;
    if (ok) ok = TIFFFlush(tif);
    TIFFCleanup(tif);
    return ok;
}

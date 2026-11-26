/****************************************************************************
    pnm.c - read and write pnm images using libpnm/netpbm routines.
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
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include <pnm.h>

#include "ImageReadLimits.h"

struct pnm_input
{
    xel *row;
    unsigned char *pixels;
};

static unsigned char
pnm_byte(xelval value, xelval maxval)
{
    return (unsigned char) (((uint64_t) value * 255 + maxval / 2) / maxval);
}

static int
decode_pnm(FILE *file, struct pnm_input *input, int *width, int *height)
{
    jmp_buf jump;
    if (setjmp(jump)) return 0;
    pm_setjmpbuf(&jump);

    int w, h, format;
    xelval maxval;
    pnm_readpnminit(file, &w, &h, &maxval, &format);
    size_t area;
    if (!image_read_area(w, h, &area) || maxval == 0) return 0;

    input->row = malloc((size_t) w * sizeof(*input->row));
    input->pixels = malloc(3 * area);
    if (input->row == NULL || input->pixels == NULL) return 0;
    for (int y = 0; y < h; ++y)
    {
        pnm_readpnmrow(file, input->row, w, maxval, format);
        for (int x = 0; x < w; ++x)
        {
            unsigned char *out = input->pixels + 3 * (y * w + x);
            const xel pixel = input->row[x];
            if (PNM_FORMAT_TYPE(format) == PPM_TYPE)
            {
                out[0] = pnm_byte(PPM_GETR(pixel), maxval);
                out[1] = pnm_byte(PPM_GETG(pixel), maxval);
                out[2] = pnm_byte(PPM_GETB(pixel), maxval);
            }
            else
                out[0] = out[1] = out[2] = pnm_byte(PNM_GET1(pixel), maxval);
        }
    }

    *width = w;
    *height = h;
    return 1;
}

int
read_pnm(const char *filename, int *width, int *height, unsigned char **rgb)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) return 0;
    struct pnm_input input = { NULL, NULL };
    const int ok = decode_pnm(file, &input, width, height);
    pm_setjmpbuf(NULL);
    fclose(file);
    free(input.row);
    if (ok) *rgb = input.pixels;
    else free(input.pixels);
    return ok;
}

int
write_pnm(FILE *outfile, int width, int height, unsigned char *rgb,
          int maxv, int format, int forceplain)
{
    xelval maxval = maxv;
    jmp_buf jump;
    xel *row = calloc(width, sizeof(*row));
    if (row == NULL) return 0;
    if (setjmp(jump))
    {
        pm_setjmpbuf(NULL);
        free(row);
        return 0;
    }
    pm_setjmpbuf(&jump);
    pnm_writepnminit(outfile, width, height, maxval, format, forceplain);
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            const unsigned char *pixel = rgb + 3 * (i * width + j);
            const xelval avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
            if (format == PBM_TYPE)
                PNM_ASSIGN1(row[j], avg > 127 ? 1 : 0);
            else if (format == PGM_TYPE)
                PNM_ASSIGN1(row[j], avg);
            else
                PNM_ASSIGN(row[j], pixel[0], pixel[1], pixel[2]);
        }
        pnm_writepnmrow(outfile, row, width, maxval, format, forceplain);
    }
    pm_setjmpbuf(NULL);
    free(row);
    return 1;
}

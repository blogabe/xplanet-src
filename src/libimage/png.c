/****************************************************************************
    png.c - read and write png images using libpng routines.
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
#include <string.h>

#include <png.h>

#include "ImageReadLimits.h"

struct png_input
{
    png_bytepp rows;
    unsigned char *pixels;
    unsigned char *alpha;
};

static int
decode_png(FILE *file, png_structp png, png_infop info, struct png_input *input,
           int *width, int *height)
{
    if (setjmp(png_jmpbuf(png))) return 0;

    png_init_io(png, file);
    png_read_info(png, info);
    const png_uint_32 w = png_get_image_width(png, info);
    const png_uint_32 h = png_get_image_height(png, info);
    size_t area;
    if (!image_read_area(w, h, &area)) return 0;

    png_set_expand(png);
    png_set_strip_16(png);
    png_set_gray_to_rgb(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    const int channels = png_get_channels(png, info);
    const size_t stride = png_get_rowbytes(png, info);
    if ((channels != 3 && channels != 4) || stride != (size_t) w * channels)
        return 0;

    input->pixels = malloc(stride * h);
    input->rows = malloc(h * sizeof(*input->rows));
    if (input->pixels == NULL || input->rows == NULL) return 0;
    for (png_uint_32 y = 0; y < h; ++y)
        input->rows[y] = input->pixels + y * stride;
    png_read_image(png, input->rows);
    png_read_end(png, NULL);

    if (channels == 4)
    {
        input->alpha = malloc(area);
        if (input->alpha == NULL) return 0;
        for (size_t i = 0; i < area; ++i)
        {
            input->alpha[i] = input->pixels[4*i + 3];
            memmove(input->pixels + 3*i, input->pixels + 4*i, 3);
        }
    }

    *width = w;
    *height = h;
    return 1;
}

int
read_png(const char *filename, int *width, int *height, unsigned char **rgb,
         unsigned char **alpha)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) return 0;
    struct png_input input = { NULL, NULL, NULL };
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png == NULL ? NULL : png_create_info_struct(png);
    const int ok = info != NULL && decode_png(file, png, info, &input, width, height);

    if (png != NULL) png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    free(input.rows);
    if (ok)
    {
        *rgb = input.pixels;
        *alpha = input.alpha;
    }
    else
    {
        free(input.pixels);
        free(input.alpha);
    }
    return ok;
}

int
write_png(FILE *outfile, int width, int height, unsigned char *rgb,
          unsigned char *alpha)
{
    unsigned char *rgba = alpha == NULL ? NULL : malloc((size_t) width * 4);
    if (alpha != NULL && rgba == NULL) return 0;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png == NULL ? NULL : png_create_info_struct(png);
    if (info == NULL)
    {
        if (png != NULL) png_destroy_write_struct(&png, NULL);
        free(rgba);
        return 0;
    }
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        free(rgba);
        return 0;
    }

    png_init_io(png, outfile);
    png_set_IHDR(png, info, width, height, 8,
                 alpha == NULL ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int i = 0; i < height; ++i)
    {
        unsigned char *row = rgb + 3 * i * width;
        if (alpha != NULL)
        {
            for (int j = 0; j < width; ++j)
            {
                memcpy(rgba + 4*j, row + 3*j, 3);
                rgba[4*j + 3] = alpha[i * width + j];
            }
            row = rgba;
        }
        png_write_row(png, row);
    }
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    free(rgba);
    return 1;
}

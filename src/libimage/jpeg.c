/****************************************************************************
    jpeg.c - read and write jpeg images using libjpeg routines
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

#include <jpeglib.h>
#include <jerror.h>

#include "ImageReadLimits.h"

struct jpeg_error_bridge
{
    struct jpeg_error_mgr manager;
    jmp_buf jump;
};

static void
jpeg_codec_failed(j_common_ptr cinfo)
{
    struct jpeg_error_bridge *error = (struct jpeg_error_bridge *) cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(error->jump, 1);
}

static void
jpeg_read_message(j_common_ptr cinfo, int level)
{
    if (level >= 0) return;
    switch (cinfo->err->msg_code)
    {
    case JWRN_EXTRANEOUS_DATA:
    case JWRN_NOT_SEQUENTIAL:
        /* The decoder can still recover every sample in these cases. */
        return;
    case JWRN_JPEG_EOF:
    {
        const j_decompress_ptr codec = (j_decompress_ptr) cinfo;
        /* Accept an omitted EOI only for a complete single sequential scan.
         * If the synthesized EOI interrupts entropy data, libjpeg then emits
         * JWRN_HIT_MARKER, which is fatal below. */
        if (!codec->progressive_mode && codec->input_scan_number == 1
            && codec->comps_in_scan == codec->num_components)
            return;
        break;
    }
    default:
        break;
    }
    jpeg_codec_failed(cinfo);
}

struct jpeg_input
{
    struct jpeg_decompress_struct codec;
    struct jpeg_error_bridge error;
    unsigned char *pixels;
};

static int
decode_jpeg(FILE *file, struct jpeg_input *input, int *width, int *height)
{
    struct jpeg_decompress_struct *codec = &input->codec;
    codec->err = jpeg_std_error(&input->error.manager);
    input->error.manager.error_exit = jpeg_codec_failed;
    input->error.manager.emit_message = jpeg_read_message;
    if (setjmp(input->error.jump)) return 0;

    jpeg_create_decompress(codec);
    jpeg_stdio_src(codec, file);
    if (jpeg_read_header(codec, TRUE) != JPEG_HEADER_OK) return 0;
    if (codec->jpeg_color_space == JCS_CMYK || codec->jpeg_color_space == JCS_YCCK)
    {
        fprintf(stderr, "CMYK JPEG maps are unsupported; convert to RGB.\n");
        return 0;
    }

    size_t area;
    codec->out_color_space = JCS_RGB;
    jpeg_calc_output_dimensions(codec);
    if (!image_read_area(codec->output_width, codec->output_height, 3, "JPEG", &area))
        return 0;
    if (!jpeg_start_decompress(codec) || codec->output_components != 3)
        return 0;

    input->pixels = malloc(3 * area);
    if (input->pixels == NULL) return 0;
    while (codec->output_scanline < codec->output_height)
    {
        JSAMPROW row = input->pixels + 3 * (size_t) codec->output_width
                                      * codec->output_scanline;
        if (jpeg_read_scanlines(codec, &row, 1) != 1) return 0;
    }

    if (!jpeg_finish_decompress(codec)) return 0;
    *width = codec->output_width;
    *height = codec->output_height;
    return 1;
}

int
read_jpeg(const char *filename, int *width, int *height, unsigned char **rgb)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) return 0;
    struct jpeg_input *input = calloc(1, sizeof(*input));
    if (input == NULL)
    {
        fclose(file);
        return 0;
    }

    const int ok = decode_jpeg(file, input, width, height);
    jpeg_destroy_decompress(&input->codec);
    fclose(file);
    if (ok) *rgb = input->pixels;
    else free(input->pixels);
    free(input);
    return ok;
}

int
write_jpeg(FILE *outfile, int width, int height, unsigned char *rgb,
           int quality)
{
    struct jpeg_compress_struct *cinfo = calloc(1, sizeof(*cinfo));
    struct jpeg_error_bridge error;
    JSAMPROW scanline[1];

    if (cinfo == NULL) return 0;
    cinfo->err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_codec_failed;
    if (setjmp(error.jump))
    {
        jpeg_destroy_compress(cinfo);
        free(cinfo);
        return 0;
    }
    jpeg_create_compress(cinfo);
    jpeg_stdio_dest(cinfo, outfile);

    cinfo->image_width = width;
    cinfo->image_height = height;
    cinfo->input_components = 3;
    cinfo->in_color_space = JCS_RGB;

    jpeg_set_defaults(cinfo);

    jpeg_set_quality(cinfo, quality, TRUE);

    jpeg_start_compress(cinfo, TRUE);

    while (cinfo->next_scanline < (unsigned int) height)
    {
        scanline[0] = rgb + 3 * width * cinfo->next_scanline;
        jpeg_write_scanlines(cinfo, scanline, 1);
    }

    jpeg_finish_compress(cinfo);
    jpeg_destroy_compress(cinfo);
    free(cinfo);

    return(1);
}

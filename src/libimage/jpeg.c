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

#define MAX_DIMENSION 21600

int
read_jpeg(const char *filename, int *width, int *height, unsigned char **rgb)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    unsigned char *ptr = NULL;
    unsigned int i, ipos;

    FILE *infile = fopen(filename, "rb");

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;

    /* Prevent against integer overflow - thanks to Niels Heinen */
    if (cinfo.output_width > MAX_DIMENSION 
        || cinfo.output_height > MAX_DIMENSION) 
    {
       fprintf(stderr, "Width, height in JPEG header is %d, %d\n",
               cinfo.output_width, cinfo.output_height);
       return(0);
    }

    rgb[0] = malloc(3 * cinfo.output_width * cinfo.output_height);
    if (rgb[0] == NULL)
    {
        fprintf(stderr, "Can't allocate memory for JPEG file.\n");
        fclose(infile);
        return(0);
    }

    if (cinfo.output_components == 3)
    {
        ptr = rgb[0];
        while (cinfo.output_scanline < cinfo.output_height) 
        {
            jpeg_read_scanlines(&cinfo, &ptr, 1);
            ptr += 3 * cinfo.output_width;
        }
    }
    else if (cinfo.output_components == 1)
    {
        ptr = malloc(cinfo.output_width);
        if (ptr == NULL)
        {
            fprintf(stderr, "Can't allocate memory for JPEG file.\n");
            fclose(infile);
            return(0);
        }

        ipos = 0;
        while (cinfo.output_scanline < cinfo.output_height) 
        {
            jpeg_read_scanlines(&cinfo, &ptr, 1);

            for (i = 0; i < cinfo.output_width; i++)
            {
                memset(rgb[0] + ipos, ptr[i], 3);
                ipos += 3;
            }
        }

        free(ptr);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(infile);

    return(1);
}

struct jpeg_write_error
{
    struct jpeg_error_mgr manager;
    jmp_buf jump;
};

static void
jpeg_write_failed(j_common_ptr cinfo)
{
    struct jpeg_write_error *error = (struct jpeg_write_error *) cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(error->jump, 1);
}

int
write_jpeg(FILE *outfile, int width, int height, unsigned char *rgb,
           int quality)
{
    struct jpeg_compress_struct *cinfo = calloc(1, sizeof(*cinfo));
    struct jpeg_write_error error;
    JSAMPROW scanline[1];

    if (cinfo == NULL) return 0;
    cinfo->err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_write_failed;
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

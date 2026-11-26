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

int
read_pnm(const char *filename, int *width, int *height, char **rgb)
{
    FILE *infile = fopen(filename, "rb");

    xelval maxvalP;
    int formatP;

    xel **imagearray;

    int argcP = 1;
    char **argv;

    int i, j, ipos = 0;

    argv = malloc(sizeof(char **));
    if (argv == NULL)
    {
        fprintf(stderr, "Can't allocate memory in read_pnm().\n");
        return(0);
    }

    argv[0] = malloc(10);
    if (argv[0] == NULL)
    {
        fprintf(stderr, "Can't allocate memory in read_pnm().\n");
        return(0);
    }

    strcpy(argv[0], "loadimage");

    pnm_init(&argcP, argv);

    imagearray = pnm_readpnm(infile, width, height, &maxvalP, &formatP);

    rgb[0] = malloc(3 * *width * *height);
    if (rgb[0] == NULL)
    {
        fprintf(stderr, "Can't allocate memory for PNM file.\n");
        return(0);
    }

    switch (PNM_FORMAT_TYPE(formatP))
    {
    case PPM_TYPE:
        for (j = 0; j < *height; j++)
            for (i = 0; i < *width; i++)
            {
                rgb[0][ipos++] = (unsigned char) (PPM_GETR(imagearray[j][i]) 
                                                  & 0xff); 
                rgb[0][ipos++] = (unsigned char) (PPM_GETG(imagearray[j][i]) 
                                                  & 0xff); 
                rgb[0][ipos++] = (unsigned char) (PPM_GETB(imagearray[j][i]) 
                                                  & 0xff); 
            }
        break;
    case PGM_TYPE:
    case PBM_TYPE:
        for (j = 0; j < *height; j++)
            for (i = 0; i < *width; i++)
            {
                memset(rgb[0] + ipos, 
                       (unsigned char) (PNM_GET1(imagearray[j][i]) & 0xff), 
                       3);
                ipos +=3;
            }
        break;
    default:
        fprintf(stderr, "Unknown magic number for pnm file\n");
        return(0);
    }

    pnm_freearray(imagearray, *height);
    return(1);
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

/****************************************************************************
    gif.c - read and write gif images using libgif/libungif.
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
#include <stdbool.h>
#include <string.h>

#include <gif_lib.h>

/*
  Based on the gif2rgb and rgb2gif codes in the libgif distribution. 
*/

#include "ImageReadLimits.h"

static int
read_gif_row(GifFileType *gif, const ColorMapObject *map,
             GifPixelType *row, unsigned char *output, int width)
{
    if (DGifGetLine(gif, row, width) == GIF_ERROR) return 0;
    for (int x = 0; x < width; ++x)
    {
        if (row[x] >= map->ColorCount) return 0;
        const GifColorType *color = map->Colors + row[x];
        output[3*x] = color->Red;
        output[3*x + 1] = color->Green;
        output[3*x + 2] = color->Blue;
    }
    return 1;
}

int
read_gif(const char *filename, int *width, int *height, unsigned char **rgb)
{
#if GIFLIB_MAJOR >= 5
    GifFileType *gif = DGifOpenFileName(filename, NULL);
#else
    GifFileType *gif = DGifOpenFileName(filename);
#endif
    if (gif == NULL) return 0;

    const int w = gif->SWidth, h = gif->SHeight;
    size_t area;
    unsigned char *pixels = NULL;
    GifPixelType *row = NULL;
    int found_image = 0, ok = 0;
    if (!image_read_area(w, h, &area)) goto cleanup;
    pixels = calloc(area, 3);
    row = malloc((size_t) w * sizeof(*row));
    if (pixels == NULL || row == NULL) goto cleanup;

    const ColorMapObject *global = gif->SColorMap;
    if (global != NULL && global->Colors != NULL
        && gif->SBackGroundColor >= 0 && gif->SBackGroundColor < global->ColorCount)
    {
        const GifColorType bg = global->Colors[gif->SBackGroundColor];
        for (size_t i = 0; i < area; ++i)
        {
            pixels[3*i] = bg.Red;
            pixels[3*i + 1] = bg.Green;
            pixels[3*i + 2] = bg.Blue;
        }
    }

    GifRecordType record;
    do
    {
        if (DGifGetRecordType(gif, &record) == GIF_ERROR) goto cleanup;
        if (record == IMAGE_DESC_RECORD_TYPE)
        {
            if (DGifGetImageDesc(gif) == GIF_ERROR) goto cleanup;
            const GifImageDesc *frame = &gif->Image;
            if (frame->Left < 0 || frame->Top < 0 ||
                frame->Width <= 0 || frame->Height <= 0 ||
                frame->Width > w - frame->Left || frame->Height > h - frame->Top)
                goto cleanup;

            const ColorMapObject *map = frame->ColorMap != NULL ? frame->ColorMap : global;
            if (map == NULL || map->Colors == NULL || map->ColorCount <= 0)
                goto cleanup;

            const int offsets[] = { 0, 4, 2, 1 };
            const int jumps[] = { 8, 8, 4, 2 };
            const int passes = frame->Interlace ? 4 : 1;
            for (int pass = 0; pass < passes; ++pass)
            {
                const int step = frame->Interlace ? jumps[pass] : 1;
                for (int y = offsets[pass]; y < frame->Height; y += step)
                {
                    unsigned char *out = pixels + 3 * ((frame->Top + y) * w + frame->Left);
                    if (!read_gif_row(gif, map, row, out, frame->Width))
                        goto cleanup;
                }
            }
            found_image = 1;
        }
        else if (record == EXTENSION_RECORD_TYPE)
        {
            int code;
            GifByteType *extension;
            if (DGifGetExtension(gif, &code, &extension) == GIF_ERROR)
                goto cleanup;
            while (extension != NULL)
                if (DGifGetExtensionNext(gif, &extension) == GIF_ERROR) goto cleanup;
        }
    } while (record != TERMINATE_RECORD_TYPE);
    ok = found_image;

cleanup:
#if GIFLIB_MAJOR >= 5
    if (DGifCloseFile(gif, NULL) == GIF_ERROR) ok = 0;
#else
    if (DGifCloseFile(gif) == GIF_ERROR) ok = 0;
#endif
    free(row);
    if (ok)
    {
        *width = w;
        *height = h;
        *rgb = pixels;
    }
    else free(pixels);
    return ok;
}

enum { ExpNumOfColors = 8 };

/******************************************************************************
 * Code to quantize high resolution image into lower one. You may want to
 * peek into the following article this code is based on:
 * "Color Image Quantization for frame buffer Display", by Paul Heckbert
 * SIGGRAPH 1982 page 297-307.
 ******************************************************************************
 * History:
 * 5 Jan 90 - Version 1.0 by Gershon Elber.
 *****************************************************************************/

#define ABS(x)    ((x) > 0 ? (x) : (-(x)))

#define COLOR_ARRAY_SIZE 32768
#define BITS_PER_PRIM_COLOR 5
#define MAX_PRIM_COLOR      0x1f

static int SortRGBAxis;

typedef struct QuantizedColorType {
    GifByteType RGB[3];
    GifByteType NewColorIndex;
    long Count;
    struct QuantizedColorType *Pnext;
} QuantizedColorType;

typedef struct NewColorMapType {
    GifByteType RGBMin[3], RGBWidth[3];
    unsigned int NumEntries; /* # of QuantizedColorType in linked list below */
    unsigned long Count; /* Total number of pixels in all the entries */
    QuantizedColorType *QuantizedColors;
} NewColorMapType;

static int SubdivColorMap(NewColorMapType * NewColorSubdiv,
                          unsigned int ColorMapSize,
                          unsigned int *NewColorMapSize);
static int SortCmpRtn(const void *Entry1, const void *Entry2);

/******************************************************************************
 * Quantize high resolution image into lower one. Input image consists of a
 * 2D array for each of the RGB colors with size Width by Height. There is no
 * Color map for the input. Output is a quantized image with 2D array of
 * indexes into the output color map.
 *   Note input image can be 24 bits at the most (8 for red/green/blue) and
 * the output has 256 colors at the most (256 entries in the color map.).
 * ColorMapSize specifies size of color map up to 256 and will be updated to
 * real size before returning.
 *   Also non of the parameter are allocated by this routine.
 *   This function returns GIF_OK if succesfull, GIF_ERROR otherwise.
 ******************************************************************************/
int
QuantizeBuffer(unsigned int Width,
               unsigned int Height,
               int *ColorMapSize,
               GifByteType * RedInput,
               GifByteType * GreenInput,
               GifByteType * BlueInput,
               GifByteType * OutputBuffer,
               GifColorType * OutputColorMap) {

    unsigned int Index, NumOfEntries;
    int i, j, MaxRGBError[3];
    unsigned int NewColorMapSize;
    long Red, Green, Blue;
    NewColorMapType NewColorSubdiv[256];
    QuantizedColorType *ColorArrayEntries, *QuantizedColor;

    ColorArrayEntries = (QuantizedColorType *)malloc(
                           sizeof(QuantizedColorType) * COLOR_ARRAY_SIZE);
    if (ColorArrayEntries == NULL) {
        return GIF_ERROR;
    }

    for (i = 0; i < COLOR_ARRAY_SIZE; i++) {
        ColorArrayEntries[i].RGB[0] = i >> (2 * BITS_PER_PRIM_COLOR);
        ColorArrayEntries[i].RGB[1] = (i >> BITS_PER_PRIM_COLOR) &
           MAX_PRIM_COLOR;
        ColorArrayEntries[i].RGB[2] = i & MAX_PRIM_COLOR;
        ColorArrayEntries[i].Count = 0;
    }

    /* Sample the colors and their distribution: */
    for (i = 0; i < (int)(Width * Height); i++) {
        Index = ((RedInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                  (2 * BITS_PER_PRIM_COLOR)) +
                ((GreenInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                  BITS_PER_PRIM_COLOR) +
                (BlueInput[i] >> (8 - BITS_PER_PRIM_COLOR));
        ColorArrayEntries[Index].Count++;
    }

    /* Put all the colors in the first entry of the color map, and call the
     * recursive subdivision process.  */
    for (i = 0; i < 256; i++) {
        NewColorSubdiv[i].QuantizedColors = NULL;
        NewColorSubdiv[i].Count = NewColorSubdiv[i].NumEntries = 0;
        for (j = 0; j < 3; j++) {
            NewColorSubdiv[i].RGBMin[j] = 0;
            NewColorSubdiv[i].RGBWidth[j] = 255;
        }
    }

    /* Find the non empty entries in the color table and chain them: */
    for (i = 0; i < COLOR_ARRAY_SIZE; i++)
        if (ColorArrayEntries[i].Count > 0)
            break;
    QuantizedColor = NewColorSubdiv[0].QuantizedColors = &ColorArrayEntries[i];
    NumOfEntries = 1;
    while (++i < COLOR_ARRAY_SIZE)
        if (ColorArrayEntries[i].Count > 0) {
            QuantizedColor->Pnext = &ColorArrayEntries[i];
            QuantizedColor = &ColorArrayEntries[i];
            NumOfEntries++;
        }
    QuantizedColor->Pnext = NULL;

    NewColorSubdiv[0].NumEntries = NumOfEntries; /* Different sampled colors */
    NewColorSubdiv[0].Count = ((long)Width) * Height; /* Pixels */
    NewColorMapSize = 1;
    if (SubdivColorMap(NewColorSubdiv, *ColorMapSize, &NewColorMapSize) !=
        GIF_OK) {
        free((char *)ColorArrayEntries);
        return GIF_ERROR;
    }
    if (NewColorMapSize < *ColorMapSize) {
        /* And clear rest of color map: */
        for (i = NewColorMapSize; i < *ColorMapSize; i++)
            OutputColorMap[i].Red = OutputColorMap[i].Green =
                OutputColorMap[i].Blue = 0;
    }

    /* Average the colors in each entry to be the color to be used in the
     * output color map, and plug it into the output color map itself. */
    for (i = 0; i < NewColorMapSize; i++) {
        if ((j = NewColorSubdiv[i].NumEntries) > 0) {
            QuantizedColor = NewColorSubdiv[i].QuantizedColors;
            Red = Green = Blue = 0;
            while (QuantizedColor) {
                QuantizedColor->NewColorIndex = i;
                Red += QuantizedColor->RGB[0];
                Green += QuantizedColor->RGB[1];
                Blue += QuantizedColor->RGB[2];
                QuantizedColor = QuantizedColor->Pnext;
            }
            OutputColorMap[i].Red = (Red << (8 - BITS_PER_PRIM_COLOR)) / j;
            OutputColorMap[i].Green = (Green << (8 - BITS_PER_PRIM_COLOR)) / j;
            OutputColorMap[i].Blue = (Blue << (8 - BITS_PER_PRIM_COLOR)) / j;
        } else
            fprintf(stderr,
                    "Null entry in quantized color map - that's weird.\n");
    }

    /* Finally scan the input buffer again and put the mapped index in the
     * output buffer.  */
    MaxRGBError[0] = MaxRGBError[1] = MaxRGBError[2] = 0;
    for (i = 0; i < (int)(Width * Height); i++) {
        Index = ((RedInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                 (2 * BITS_PER_PRIM_COLOR)) +
                ((GreenInput[i] >> (8 - BITS_PER_PRIM_COLOR)) <<
                 BITS_PER_PRIM_COLOR) +
                (BlueInput[i] >> (8 - BITS_PER_PRIM_COLOR));
        Index = ColorArrayEntries[Index].NewColorIndex;
        OutputBuffer[i] = Index;
        if (MaxRGBError[0] < ABS(OutputColorMap[Index].Red - RedInput[i]))
            MaxRGBError[0] = ABS(OutputColorMap[Index].Red - RedInput[i]);
        if (MaxRGBError[1] < ABS(OutputColorMap[Index].Green - GreenInput[i]))
            MaxRGBError[1] = ABS(OutputColorMap[Index].Green - GreenInput[i]);
        if (MaxRGBError[2] < ABS(OutputColorMap[Index].Blue - BlueInput[i]))
            MaxRGBError[2] = ABS(OutputColorMap[Index].Blue - BlueInput[i]);
    }

    free((char *)ColorArrayEntries);

    *ColorMapSize = NewColorMapSize;

    return GIF_OK;
}

/******************************************************************************
 * Routine to subdivide the RGB space recursively using median cut in each
 * axes alternatingly until ColorMapSize different cubes exists.
 * The biggest cube in one dimension is subdivide unless it has only one entry.
 * Returns GIF_ERROR if failed, otherwise GIF_OK.
 ******************************************************************************/
static int
SubdivColorMap(NewColorMapType * NewColorSubdiv,
               unsigned int ColorMapSize,
               unsigned int *NewColorMapSize) {

    int MaxSize;
    unsigned int i, j, Index = 0, NumEntries, MinColor, MaxColor;
    long Sum, Count;
    QuantizedColorType *QuantizedColor, **SortArray;

    while (ColorMapSize > *NewColorMapSize) {
        /* Find candidate for subdivision: */
        MaxSize = -1;
        for (i = 0; i < *NewColorMapSize; i++) {
            for (j = 0; j < 3; j++) {
                if ((((int)NewColorSubdiv[i].RGBWidth[j]) > MaxSize) &&
                    (NewColorSubdiv[i].NumEntries > 1)) {
                    MaxSize = NewColorSubdiv[i].RGBWidth[j];
                    Index = i;
                    SortRGBAxis = j;
                }
            }
        }

        if (MaxSize == -1)
            return GIF_OK;

        /* Split the entry Index into two along the axis SortRGBAxis: */

        /* Sort all elements in that entry along the given axis and split at
         * the median.  */
        SortArray = (QuantizedColorType **)malloc(
                      sizeof(QuantizedColorType *) * 
                      NewColorSubdiv[Index].NumEntries);
        if (SortArray == NULL)
            return GIF_ERROR;
        for (j = 0, QuantizedColor = NewColorSubdiv[Index].QuantizedColors;
             j < NewColorSubdiv[Index].NumEntries && QuantizedColor != NULL;
             j++, QuantizedColor = QuantizedColor->Pnext)
            SortArray[j] = QuantizedColor;

        qsort(SortArray, NewColorSubdiv[Index].NumEntries,
              sizeof(QuantizedColorType *), SortCmpRtn);

        /* Relink the sorted list into one: */
        for (j = 0; j < NewColorSubdiv[Index].NumEntries - 1; j++)
            SortArray[j]->Pnext = SortArray[j + 1];
        SortArray[NewColorSubdiv[Index].NumEntries - 1]->Pnext = NULL;
        NewColorSubdiv[Index].QuantizedColors = QuantizedColor = SortArray[0];
        free((char *)SortArray);

        /* Now simply add the Counts until we have half of the Count: */
        Sum = NewColorSubdiv[Index].Count / 2 - QuantizedColor->Count;
        NumEntries = 1;
        Count = QuantizedColor->Count;
        while (QuantizedColor->Pnext != NULL &&
               (Sum -= QuantizedColor->Pnext->Count) >= 0 &&
               QuantizedColor->Pnext->Pnext != NULL) {
            QuantizedColor = QuantizedColor->Pnext;
            NumEntries++;
            Count += QuantizedColor->Count;
        }
        /* Save the values of the last color of the first half, and first
         * of the second half so we can update the Bounding Boxes later.
         * Also as the colors are quantized and the BBoxes are full 0..255,
         * they need to be rescaled.
         */
        MaxColor = QuantizedColor->RGB[SortRGBAxis]; /* Max. of first half */
        /* coverity[var_deref_op] */
        MinColor = QuantizedColor->Pnext->RGB[SortRGBAxis]; /* of second */
        MaxColor <<= (8 - BITS_PER_PRIM_COLOR);
        MinColor <<= (8 - BITS_PER_PRIM_COLOR);

        /* Partition right here: */
        NewColorSubdiv[*NewColorMapSize].QuantizedColors =
            QuantizedColor->Pnext;
        QuantizedColor->Pnext = NULL;
        NewColorSubdiv[*NewColorMapSize].Count = Count;
        NewColorSubdiv[Index].Count -= Count;
        NewColorSubdiv[*NewColorMapSize].NumEntries =
            NewColorSubdiv[Index].NumEntries - NumEntries;
        NewColorSubdiv[Index].NumEntries = NumEntries;
        for (j = 0; j < 3; j++) {
            NewColorSubdiv[*NewColorMapSize].RGBMin[j] =
                NewColorSubdiv[Index].RGBMin[j];
            NewColorSubdiv[*NewColorMapSize].RGBWidth[j] =
                NewColorSubdiv[Index].RGBWidth[j];
        }
        NewColorSubdiv[*NewColorMapSize].RGBWidth[SortRGBAxis] =
            NewColorSubdiv[*NewColorMapSize].RGBMin[SortRGBAxis] +
            NewColorSubdiv[*NewColorMapSize].RGBWidth[SortRGBAxis] - MinColor;
        NewColorSubdiv[*NewColorMapSize].RGBMin[SortRGBAxis] = MinColor;

        NewColorSubdiv[Index].RGBWidth[SortRGBAxis] =
            MaxColor - NewColorSubdiv[Index].RGBMin[SortRGBAxis];

        (*NewColorMapSize)++;
    }

    return GIF_OK;
}

/****************************************************************************
 * Routine called by qsort to compare two entries.
 ****************************************************************************/
static int
SortCmpRtn(const void *Entry1,
           const void *Entry2) {

    return (*((QuantizedColorType **) Entry1))->RGB[SortRGBAxis] -
       (*((QuantizedColorType **) Entry2))->RGB[SortRGBAxis];
}

/******************************************************************************
* Close output file (if open), and exit.
******************************************************************************/
static void QuitGifError(GifFileType *GifFile)
{
    fprintf(stderr, "Error writing GIF file\n");
#if GIFLIB_MAJOR >= 5
    if (GifFile != NULL) EGifCloseFile(GifFile, NULL);
#else
    if (GifFile != NULL) EGifCloseFile(GifFile);
#endif
}

static int
write_gif_bytes(GifFileType *gif, const GifByteType *bytes, int count)
{
    return (int) fwrite(bytes, 1, count, gif->UserData);
}

int
write_gif(FILE *file, int width, int height, unsigned char *rgb)
{
    if (width > 65535 || height > 65535) return 0;

    const int area = width * height;
    int colors = 1 << ExpNumOfColors;
#if GIFLIB_MAJOR >= 5
    ColorMapObject *map = GifMakeMapObject(colors, NULL);
#else
    ColorMapObject *map = MakeMapObject(colors, NULL);
#endif
    GifByteType *indices = malloc(area);
    GifByteType *planes = malloc(3 * area);
    int ok = 0;
    if (map == NULL || indices == NULL || planes == NULL) goto cleanup;

    for (int i = 0; i < area; ++i)
    {
        planes[i] = rgb[3*i];
        planes[area + i] = rgb[3*i + 1];
        planes[2*area + i] = rgb[3*i + 2];
    }
    if (QuantizeBuffer(width, height, &colors, planes, planes + area,
                       planes + 2*area, indices, map->Colors) == GIF_ERROR)
        goto cleanup;

#if GIFLIB_MAJOR >= 5
    GifFileType *gif = EGifOpen(file, write_gif_bytes, NULL);
#else
    GifFileType *gif = EGifOpen(file, write_gif_bytes);
#endif
    if (gif == NULL) goto cleanup;

    ok = EGifPutScreenDesc(gif, width, height, ExpNumOfColors, 0, map) != GIF_ERROR
        && EGifPutImageDesc(gif, 0, 0, width, height, false, NULL) != GIF_ERROR;
    for (int i = 0; ok && i < height; ++i)
        ok = EGifPutLine(gif, indices + i * width, width) != GIF_ERROR;

#if GIFLIB_MAJOR >= 5
    if (EGifCloseFile(gif, NULL) == GIF_ERROR) ok = 0;
#else
    if (EGifCloseFile(gif) == GIF_ERROR) ok = 0;
#endif
cleanup:
    free(indices);
    free(planes);
    if (map != NULL)
    {
#if GIFLIB_MAJOR >= 5
        GifFreeMapObject(map);
#else
        FreeMapObject(map);
#endif
    }
    return ok;
}

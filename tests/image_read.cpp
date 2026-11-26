#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "config.h"
#include "libimage/Image.h"
#include "libimage/ImageReadLimits.h"

#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
#endif

namespace
{
void
require(bool ok, const char *message)
{
    if (ok) return;
    std::cerr << message << '\n';
    std::exit(1);
}

#if defined(HAVE_LIBPNG) || defined(HAVE_LIBJPEG) || defined(HAVE_LIBGIF) \
    || defined(HAVE_LIBTIFF) || defined(HAVE_LIBPNM)
void
roundTrip(Image &input, Image &output, const char *path, int tolerance)
{
    require(input.Write(path) && output.Read(path), path);
    require(output.Width() == 16 && output.Height() == 16, "Round-trip dimensions");
    const unsigned char *pixels = output.getRGBData();
    require(pixels != NULL
            && std::abs(pixels[0] - 40) <= tolerance
            && std::abs(pixels[3*255] - 200) <= tolerance,
            "Round-trip channel values or orientation");
}
#endif

#ifdef HAVE_LIBPNG
void
writePngFixture(const char *path, bool palette)
{
    FILE *file = std::fopen(path, "wb");
    require(file != NULL, "Open PNG fixture");
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    require(png != NULL, "Create PNG fixture");
    png_infop info = png_create_info_struct(png);
    require(info != NULL, "Create PNG fixture info");
    if (setjmp(png_jmpbuf(png))) require(false, "Write PNG fixture");

    png_init_io(png, file);
    png_set_IHDR(png, info, 2, 1, palette ? 1 : 16,
                 palette ? PNG_COLOR_TYPE_PALETTE : PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_color colors[2] = { {12, 34, 56}, {78, 90, 123} };
    png_byte transparency[2] = { 0, 255 };
    png_byte indexed[] = { 0x40 };
    png_byte grayscale[] = { 0xab, 0xcd, 0x03, 0xff };
    if (palette)
    {
        png_set_PLTE(png, info, colors, 2);
        png_set_tRNS(png, info, transparency, 2, NULL);
    }
    png_write_info(png, info);
    png_write_row(png, palette ? indexed : grayscale);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    require(std::fclose(file) == 0, "Close PNG fixture");
}
#endif

#ifdef HAVE_LIBJPEG
void
writeGrayJpegFixture()
{
    FILE *file = std::fopen("gray.jpg", "wb");
    require(file != NULL, "Open grayscale JPEG fixture");
    jpeg_compress_struct jpeg;
    jpeg_error_mgr error;
    jpeg.err = jpeg_std_error(&error);
    jpeg_create_compress(&jpeg);
    jpeg_stdio_dest(&jpeg, file);
    jpeg.image_width = 8;
    jpeg.image_height = 8;
    jpeg.input_components = 1;
    jpeg.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults(&jpeg);
    jpeg_start_compress(&jpeg, TRUE);
    unsigned char row[8];
    std::memset(row, 73, sizeof(row));
    while (jpeg.next_scanline < jpeg.image_height)
    {
        JSAMPROW ptr = row;
        require(jpeg_write_scanlines(&jpeg, &ptr, 1) == 1, "Write grayscale JPEG fixture");
    }
    jpeg_finish_compress(&jpeg);
    jpeg_destroy_compress(&jpeg);
    require(std::fclose(file) == 0, "Close grayscale JPEG fixture");
}
#endif
}

int
main()
{
    size_t area = 0;
    require(image_read_area(21600, 10800, 5, "PNG", &area)
            && area == 233280000, "Global relief dimensions must fit");
    require(!image_read_area(32768, 32768, 5, "PNG", &area),
            "Large decoded rasters must still be bounded");
    unsigned char rgb[3 * 256], alpha[256];
    for (int i = 0; i < 256; ++i)
    {
        rgb[3*i] = i < 128 ? 40 : 200;
        rgb[3*i + 1] = 64;
        rgb[3*i + 2] = 192;
        alpha[i] = i;
    }
    Image input(16, 16, rgb, alpha);
    const unsigned char *previousRgb = input.getRGBData();
    const unsigned char *previousAlpha = input.getPNGAlpha();
    FILE *empty = std::fopen("empty", "wb");
    require(empty != NULL && std::fclose(empty) == 0, "Create empty input");
    require(!input.Read("not-present"), "Missing image must fail");
    require(!input.Read("empty"), "Empty image must fail");
    require(input.Width() == 16 && input.Height() == 16
            && input.getRGBData() == previousRgb && input.getPNGAlpha() == previousAlpha,
            "Failed read changed the existing image");

    Image output;
#ifdef HAVE_LIBPNG
    roundTrip(input, output, "image.png", 0);
    require(output.getPNGAlpha() != NULL && output.getPNGAlpha()[111] == 111,
            "RGBA PNG did not retain alpha");
    writePngFixture("palette.png", true);
    require(output.Read("palette.png") && output.Width() == 2
            && output.getRGBData()[0] == 12 && output.getRGBData()[3] == 78
            && output.getPNGAlpha() != NULL
            && output.getPNGAlpha()[0] == 0 && output.getPNGAlpha()[1] == 255,
            "Palette PNG did not expand color and transparency");
    writePngFixture("gray16.png", false);
    require(output.Read("gray16.png") && output.Width() == 2
            && output.getRGBData()[0] == 0xab && output.getRGBData()[2] == 0xab
            && output.getRGBData()[3] == 0x03 && output.getPNGAlpha() == NULL,
            "16-bit grayscale PNG did not expand to RGB or clear old alpha");
#endif
#ifdef HAVE_LIBJPEG
    roundTrip(input, output, "image.jpg", 10);
    writeGrayJpegFixture();
    require(output.Read("gray.jpg") && output.Width() == 8, "Read grayscale JPEG");
    const unsigned char *gray = output.getRGBData();
    require(gray != NULL && std::abs(gray[0] - 73) <= 2
            && gray[0] == gray[1] && gray[1] == gray[2] && output.getPNGAlpha() == NULL,
            "Grayscale JPEG did not expand to RGB");
#endif
#ifdef HAVE_LIBGIF
    roundTrip(input, output, "image.gif", 10);
#endif
#ifdef HAVE_LIBTIFF
    roundTrip(input, output, "image.tiff", 0);
#endif
#ifdef HAVE_LIBPNM
    roundTrip(input, output, "image.ppm", 0);
#endif
    const unsigned char solid[] = { 200, 100, 50, 200, 100, 50,
                                    200, 100, 50, 200, 100, 50 };
    const unsigned char asym[] = { 0, 0, 100, 28 };
    Image sample(2, 2, solid, asym);
    unsigned char pixel[3], opacity;
    sample.getPixel(0.5, 0.5, pixel, &opacity);
    require(pixel[0] == 200 && pixel[1] == 100 && pixel[2] == 50 && opacity == 32,
            "Interpolation changed a constant color or selected the wrong alpha");
    sample.getPixel(0, 1, pixel, &opacity);
    require(opacity == 100, "Interpolation changed a pixel-center alpha");
    Image opaque(2, 2, solid, NULL);
    opaque.getPixel(0.5, 0.5, pixel, &opacity);
    require(opacity == 255, "Image without an alpha channel must sample as opaque");
    std::cout << "Image read and sampling cases passed\n";
}

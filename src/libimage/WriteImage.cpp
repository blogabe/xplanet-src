#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

#include "config.h"

extern "C"
{
    int write_bmp(FILE *, int, int, unsigned char *);
#ifdef HAVE_LIBGIF
    int write_gif(FILE *, int, int, unsigned char *);
#endif
#ifdef HAVE_LIBJPEG
    int write_jpeg(FILE *, int, int, unsigned char *, int);
#endif
#ifdef HAVE_LIBPNG
    int write_png(FILE *, int, int, unsigned char *, unsigned char *);
#endif
#ifdef HAVE_LIBPNM
#include <pnm.h>
    int write_pnm(FILE *, int, int, unsigned char *, int, int, int);
#endif
#ifdef HAVE_LIBTIFF
    int write_tiff(FILE *, int, int, unsigned char *);
#endif
}

namespace
{
enum Format { BMP, GIF, JPEG, PNG, PBM, PGM, PPM, TIFF, Unknown };

Format
imageFormat(const string &extension)
{
    if (extension == ".bmp") return BMP;
#ifdef HAVE_LIBGIF
    if (extension == ".gif") return GIF;
#endif
#ifdef HAVE_LIBJPEG
    if (extension == ".jpg" || extension == ".jpeg") return JPEG;
#endif
#ifdef HAVE_LIBPNG
    if (extension == ".png") return PNG;
#endif
#ifdef HAVE_LIBPNM
    if (extension == ".pbm") return PBM;
    if (extension == ".pgm") return PGM;
    if (extension == ".ppm") return PPM;
#endif
#ifdef HAVE_LIBTIFF
    if (extension == ".tif" || extension == ".tiff") return TIFF;
#endif
    return Unknown;
}

bool
encodeImage(Format format, FILE *file, int width, int height,
            unsigned char *rgb, unsigned char *alpha, int quality)
{
    switch (format)
    {
    case BMP: return write_bmp(file, width, height, rgb) == 1;
#ifdef HAVE_LIBGIF
    case GIF: return write_gif(file, width, height, rgb) == 1;
#endif
#ifdef HAVE_LIBJPEG
    case JPEG: return write_jpeg(file, width, height, rgb, quality) == 1;
#endif
#ifdef HAVE_LIBPNG
    case PNG: return write_png(file, width, height, rgb, alpha) == 1;
#endif
#ifdef HAVE_LIBPNM
    case PBM: return write_pnm(file, width, height, rgb, 1, PBM_TYPE, 0) == 1;
    case PGM: return write_pnm(file, width, height, rgb, 255, PGM_TYPE, 0) == 1;
    case PPM: return write_pnm(file, width, height, rgb, 255, PPM_TYPE, 0) == 1;
#endif
#ifdef HAVE_LIBTIFF
    case TIFF: return write_tiff(file, width, height, rgb) == 1;
#endif
    default: return false;
    }
}
}

bool
WriteImage(const char *filename, const int width, const int height,
                unsigned char * const rgb_data, unsigned char * const png_alpha,
                const int quality)
{
    if (filename == NULL || *filename == '\0' || rgb_data == NULL ||
        width <= 0 || height <= 0 || width > (INT_MAX / 4) / height)
        return false;

    const string path(filename);
    const size_t slash = path.rfind('/');
    const size_t base = slash == string::npos ? 0 : slash + 1;
    const size_t dot = path.rfind('.');
    string extension;
    if (dot != string::npos && dot >= base) extension = path.substr(dot);
    for (size_t i = 0; i < extension.size(); ++i)
        extension[i] = static_cast<char>(tolower(static_cast<unsigned char>(extension[i])));
    const Format format = imageFormat(extension);
    if (format == Unknown)
    {
        fprintf(stderr, "Unsupported image output format for %s\n", filename);
        return false;
    }

    struct stat existing;
    mode_t mode;
    if (lstat(filename, &existing) == 0)
    {
        if (!S_ISREG(existing.st_mode))
        {
            fprintf(stderr, "Image output must be a regular file: %s\n", filename);
            return false;
        }
        if (access(filename, W_OK) != 0)
        {
            perror(filename);
            return false;
        }
        mode = existing.st_mode & 0777;
    }
    else if (errno == ENOENT)
    {
        // Match normal file creation in this single-threaded renderer.
        const mode_t mask = umask(0777);
        umask(mask);
        mode = 0666 & ~mask;
    }
    else
    {
        perror(filename);
        return false;
    }

    // A sibling temp file keeps the final rename on the same filesystem.
    const string tempPath = path.substr(0, base) + ".xplanet-image.XXXXXX";
    vector<char> temp(tempPath.begin(), tempPath.end());
    temp.push_back('\0');
    const int fd = mkstemp(&temp[0]);
    if (fd < 0)
    {
        perror(filename);
        return false;
    }
    FILE *file = fdopen(fd, "w+b");
    if (file == NULL)
    {
        perror(filename);
        close(fd);
        unlink(&temp[0]);
        return false;
    }

    bool ok = encodeImage(format, file, width, height, rgb_data, png_alpha, quality);
    if (fflush(file) != 0 || ferror(file)) ok = false;
    if (ok && fchmod(fd, mode) != 0)
    {
        perror(filename);
        ok = false;
    }
    if (fclose(file) != 0) ok = false;
    if (ok && rename(&temp[0], filename) != 0)
    {
        perror(filename);
        ok = false;
    }
    if (!ok) unlink(&temp[0]);
    return ok;
}

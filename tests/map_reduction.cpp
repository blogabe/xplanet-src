#include <climits>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "MapReduction.h"

namespace
{
void
require(bool ok, const char *message)
{
    if (ok) return;
    std::cerr << message << '\n';
    std::exit(1);
}

void
checkCase(int width, int height, int factor, int w, int h)
{
    std::vector<unsigned char> source(3 * width * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const int p = 3 * (y * width + x);
            source[p] = x;
            source[p + 1] = y;
            source[p + 2] = 200;
        }

    int actualW = 0, actualH = 0;
    unsigned char *output = reduceMapRGB(&source[0], width, height, factor,
                                         actualW, actualH);
    require(output != NULL && actualW == w && actualH == h,
            "Incorrect reduced map dimensions");
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const int p = 3 * (y * w + x);
            const int firstX = x * width / w;
            const int lastX = (x + 1) * width / w - 1;
            const int firstY = y * height / h;
            const int lastY = (y + 1) * height / h - 1;
            require(output[p] == (firstX + lastX) / 2
                    && output[p + 1] == (firstY + lastY) / 2
                    && output[p + 2] == 200,
                    "Incorrect reduced map color or orientation");
        }
    delete [] output;
}
}

int
main()
{
    checkCase(64, 64, 3, 16, 16); // Square stays square at the width limit.
    checkCase(64, 32, 3, 16, 8);  // Conventional 2:1 map.
    checkCase(32, 64, 1, 16, 32); // Portrait map keeps its orientation.
    checkCase(35, 5, 1, 17, 2);   // Right and bottom remainder.
    checkCase(32, 1, INT_MAX, 16, 1); // One row and a bounded scale.
    const unsigned char tiny[3] = { 17, 18, 19 };
    int w = -1, h = -1;
    require(reduceMapRGB(tiny, 1, 1, 2, w, h) == NULL && w == -1 && h == -1,
            "A tiny map must be left alone");
    require(reduceMapRGB(tiny, 1, 1, 0, w, h) == NULL,
            "A zero factor must do nothing");
    std::cout << "Map reduction cases passed\n";
}

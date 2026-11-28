#include <cstdio>

#include "ParseToken.h"

int main()
{
    const struct
    {
        const char *token;
        int start;
        bool commentEndsLine;
        int expected;
    } cases[] = {
        {"setting=value", 0, true, 7},
        {"x=setting=value", 2, true, 9},
        {"setting=", 0, true, 7},
        {"setting", 0, true, -1},
        {"setting", 7, true, -1},
        {"setting#note=value", 0, true, -1},
        {"setting#note=value", 0, false, 12},
        {"setting\rnote=value", 0, true, -1},
        {"setting\034note=value", 0, true, -1},
    };
    for (const auto &test : cases)
    {
        const int result = parseToken::findSeparator(
            test.token, test.start, '=', test.commentEndsLine);
        if (result != test.expected)
        {
            std::fprintf(stderr, "Separator test returned %d; expected %d\n",
                         result, test.expected);
            return 1;
        }
    }
    std::puts("Assignment separator boundaries passed");
}

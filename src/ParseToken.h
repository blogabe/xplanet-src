#ifndef XPLANET_PARSE_TOKEN_H
#define XPLANET_PARSE_TOKEN_H

namespace parseToken
{
inline bool isLineTerminator(char c, bool commentEndsLine)
{
    // 13 is DOS end-of-line, 28 is the file separator.
    return c == '\0' || c == 13 || c == 28
        || (commentEndsLine && c == '#');
}

// Return the absolute index of a separator within the current line.
inline int findSeparator(const char *line, int start, char separator,
                         bool commentEndsLine)
{
    for (int i = start; !isLineTerminator(line[i], commentEndsLine); ++i)
        if (line[i] == separator) return i;

    return -1;
}
}

#endif

/* Exercise the same fixed-width catalog decoder used by the renderer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sgp4sdp4.h"

static void set_id(char line[80], const char *id)
{
    int sum = 0;
    memcpy(line + 2, id, 5);
    for (int i = 0; i < 68; ++i)
    {
        if (line[i] >= '0' && line[i] <= '9')
            sum += line[i] - '0';
        else if (line[i] == '-')
            ++sum;
    }
    line[68] = '0' + sum % 10;
}

static void check_id(const char *first, const char *second, int expected)
{
    char lines[3][80] = {
        "Catalog fixture",
        "1 25544U 98067A   04005.51955591  .00019728  00000-0  19085-3 0  9701",
        "2 25544  51.6297  78.4979 0006431 344.6528 119.5204 15.66405366292649",
    };
    tle_t sat = {0};
    set_id(lines[1], first);
    set_id(lines[2], second);
    int result = Get_Next_Tle_Set(lines, &sat);
    if (expected > 0 ? result != 1 || sat.catnr != expected : result == 1)
    {
        fprintf(stderr, "Catalog case %.5s/%.5s: status %d, ID %d (expected %d)\n",
                first, second, result, sat.catnr, expected);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    check_id("25544", "25544", 25544);
    check_id("    1", "    1", 1);
    check_id("99999", "99999", 99999);
    check_id("A0000", "A0000", 100000);
    check_id("A0057", "A0057", 100057);
    check_id("J0000", "J0000", 180000);
    check_id("P0000", "P0000", 230000);
    check_id("Z9999", "Z9999", 339999);
    check_id("I0057", "I0057", 0);
    check_id("O0057", "O0057", 0);
    check_id("A0057", "A0058", 0);
    puts("TLE catalog ID cases passed");
    return EXIT_SUCCESS;
}

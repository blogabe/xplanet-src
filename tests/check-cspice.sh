#!/bin/sh
# Check the real kernel load and ephemeris-to-render path without a download.
set -eu
binary=$1
assets=$2
spice=$3
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-spice.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
cd "$scratch"
mkdir home spice
export HOME="$scratch/home" LC_ALL=C
cat > fixture.c <<'C'
#include <SpiceUsr.h>

static void write_kernel(const char *path, SpiceDouble x)
{
    SpiceInt handle;
    const SpiceDouble states[2][6] = {
        {x, 0, 0, 0, 0, 0},
        {x, 0, 0, 0, 0, 0}
    };
    spkopn_c(path, "Xplanet test", 0, &handle);
    spkw08_c(handle, 399, 10, "J2000", -86400, 86400,
             "Synthetic Earth", 1, 2, states, -86400, 172800);
    spkcls_c(handle);
}

int main(void)
{
    write_kernel("spice/positive.bsp", 149597870.7);
    write_kernel("spice/negative.bsp", -149597870.7);
    return 0;
}
C
"${CC:-cc}" -I"$spice/include" fixture.c -L"$spice/lib" -lcspice -lm -o fixture
./fixture
for side in positive negative; do
    : > "spice/$side"
    printf '%s\n' "$side.bsp" > "spice/$side.krn"
    if ! "$binary" -searchdir "$PWD" -searchdir "$assets" \
        -target earth -projection rectangular -date 20000101.120000 \
        -spice_file "$side" -spice_ephemeris 399 -geometry 160x80 \
        -num_times 1 -output "$side.png" > "$side.log" 2>&1; then
        cat "$side.log" >&2
        exit 1
    fi
    if grep -E 'SPICE\([A-Z]+\)|[Cc]an.t find spice' "$side.log"; then exit 1; fi
    header=$(od -An -tx1 -N24 "$side.png" | tr -d '[:space:]')
    [ "$header" = 89504e470d0a1a0a0000000d49484452000000a000000050 ]
done
if cmp -s positive.png negative.png; then
    echo 'Changing the SPICE Earth position did not change the render' >&2
    exit 1
fi
printf '%s\n' 'CSPICE kernel load and Earth lighting check passed'

#!/bin/sh
# Generate offline kernels, check the production ephemeris and render from them.
set -eu
binary=$1
src=$2
spice=$3
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-spice.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
cd "$scratch"
mkdir home images spice
export HOME="$scratch/home" LC_ALL=C

"${CXX:-c++}" -I"$spice/include" -I"$src/src" -I"$src/src/libephemeris" \
    "$src/tests/cspice-fixture.cpp" "$src/src/libephemeris/Ephemeris.cpp" \
    "$src/src/libephemeris/EphemerisSpice.cpp" -L"$spice/lib" -lcspice -lm -o fixture
./fixture

python3 -I - <<'PY'
from pathlib import Path
from struct import pack
from zlib import compress, crc32

def chunk(kind, data):
    return pack('>I', len(data)) + kind + data + pack('>I', crc32(kind + data))

for name, color in [('day', (160, 160, 160)), ('night', (0, 0, 0))]:
    header = pack('>IIBBBBB', 160, 80, 8, 2, 0, 0, 0)
    pixels = (bytes(1) + bytes(color) * 160) * 80
    png = bytes.fromhex('89504e470d0a1a0a') + chunk(b'IHDR', header)
    png += chunk(b'IDAT', compress(pixels)) + chunk(b'IEND', b'')
    Path(f'images/{name}.png').write_bytes(png)
PY
printf '[earth]\nmap=%s/images/day.png\nnight_map=%s/images/night.png\nshade=0\ntwilight=0\n' \
    "$PWD" "$PWD" > lighting.conf
: > spice/positive
: > spice/negative
printf '%s\n' '-31 {V} relative_to=399 color={255,0,255} symbolsize=3' > spice/spacecraft

for scene in positive negative spacecraft; do
    kernel=$scene
    if [ "$scene" = spacecraft ]; then kernel=positive; fi
    printf '%s\n' "$kernel.bsp" > "spice/$scene.krn"
    if ! "$binary" -searchdir "$PWD" -searchdir "$src/xplanet" \
        -config lighting.conf -target earth -projection rectangular \
        -latitude 0 -longitude 0 -date 20000101.120000 \
        -spice_file "$scene" -spice_ephemeris 399 -geometry 160x80 \
        -num_times 1 -output "$scene.bmp" > "$scene.log" 2>&1; then
        cat "$scene.log" >&2
        exit 1
    fi
    if grep -Ei 'SPICE\([A-Z_]+\)|can.t (find spice|compute position)|syntax error in spice|invalid NAIF' "$scene.log"; then
        exit 1
    fi
done

python3 -I - <<'PY'
from pathlib import Path
from struct import unpack_from

def open_bmp(name):
    data = Path(f'{name}.bmp').read_bytes()
    assert data[:2] == b'BM' and unpack_from('<iiHHI', data, 18) == (160, 80, 1, 24, 0), name
    offset, = unpack_from('<I', data, 10)
    assert len(data) == offset + 160 * 80 * 3, name

    def pixel(x, y):
        start = offset + ((79 - y) * 160 + x) * 3
        return tuple(reversed(data[start:start + 3]))

    return pixel

positive, negative, spacecraft = map(open_bmp, ('positive', 'negative', 'spacecraft'))
# At J2000 Greenwich sidereal angle is about 280.46 degrees. On a 160x80
# rectangular map, lon = 2.25*x - 180 and lat = 90 - 2.25*y. The dot
# products with the Sun direction separate day and night well away from
# the terminator. The samples distinguish all three pairwise axis swaps.
for x, y, positive_is_lit in ((10, 55, True), (100, 25, False), (150, 55, True)):
    for img, lit in ((positive, positive_is_lit), (negative, not positive_is_lit)):
        rgb = img(x, y)
        matches = min(rgb) >= 100 if lit else max(rgb) <= 60
        assert matches, (x, y, lit, rgb)

# The Earth-relative spacecraft vector is at 45 E, 30 N, i.e. near (100,27).
# No marker pixels should appear when rendering the same kernel with no row.
def magenta(rgb):
    r, g, b = rgb
    return r > 220 and g < 40 and b > 220

hits = [(x, y) for y in range(80) for x in range(160) if magenta(spacecraft(x, y))]
assert hits and any(abs(x - 100) <= 2 and abs(y - 27) <= 2 for x, y in hits), hits[:20]
assert not any(magenta(positive(x, y)) for x, y in hits)
PY
printf '%s\n' 'CSPICE AU coordinates, day/night geometry and spacecraft marker passed'

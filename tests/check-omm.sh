#!/bin/sh
# End-to-end satellite input checks; fixtures are synthetic and offline.
set -eu
binary=$1
assets=$2
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-omm.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
mkdir "$scratch/home"
HOME="$scratch/home"
LC_ALL=C
export HOME LC_ALL
cd "$scratch"
"$binary" -version > version.log
if ! grep -Eq '^[[:space:]]+PNG[[:space:]]*$' version.log ||
   ! grep -Eq '^[[:space:]]+JPEG[[:space:]]*$' version.log; then
    printf '%s\n' 'SKIP: OMM render check needs PNG and JPEG support'
    exit 0
fi
render()
{
    "$binary" -searchdir "$assets" -target earth -projection rectangular \
        -date "${3:-20261125.120000}" -geometry 320x160 -num_times 1 \
        -config "$1" -output "$2"
}

printf '[earth]\n' > base.conf
printf '[earth]\nsatellite_file=%s/tracked\n' "$PWD" > omm.conf
render base.conf base.png
cat > tracked.omm.csv <<'CSV'
OBJECT_NAME,NORAD_CAT_ID,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,RA_OF_ASC_NODE,ARG_OF_PERICENTER,MEAN_ANOMALY,BSTAR,MEAN_MOTION_DOT,MEAN_MOTION_DDOT
"Example, six digits",100057,2026-11-25T12:00:00.123456,15.55,0.00045,51.64,27,115,245,0.00028,0.00011,0
Example nine digits,987654321,2026-11-25T12:00:00.123456,14.89,0.00072,98,98,55,95,0.00019,0.00009,0
CSV
for id in 100057 987654321; do
    printf '%s "" image=none color=yellow altcirc=0 trail={ground,-5,10,2}\n' "$id" > tracked
    render omm.conf "$id.png" > "$id.log" 2>&1
    if cmp -s base.png "$id.png"; then
        printf 'OMM catalog ID %s did not affect the image\n' "$id" >&2
        cat "$id.log" >&2
        exit 1
    fi
done
if cmp -s 100057.png 987654321.png; then
    printf '%s\n' 'Distinct OMM objects unexpectedly rendered identically' >&2
    exit 1
fi

# Also resolve a nine-digit OMM catalog ID as a viewpoint, not just a marker.
"$binary" -searchdir "$assets" -target earth -origin norad987654321 \
    -date 20261125.120000 -geometry 320x160 -num_times 1 \
    -config omm.conf -output observer.png > observer.log 2>&1
test -s observer.png
if grep -E 'Can.t (find satellite|get position of satellite)' observer.log; then
    exit 1
fi

# An existing TLE is unchanged even if an OMM companion appears alongside it.
printf '25544 "" image=none color=red altcirc=0\n' > tracked
sed -n '1,3p' "$assets/satellites/iss.tle" > tracked.tle
mv tracked.omm.csv alternate.omm.csv
render omm.conf tle.png
mv alternate.omm.csv tracked.omm.csv
render omm.conf tle-plus-omm.png
cmp tle.png tle-plus-omm.png
if cmp -s base.png tle.png; then
    printf '%s\n' 'Legacy TLE fixture did not affect the image' >&2
    exit 1
fi

# The same orbit must render identically through the TLE and OMM code paths
# at a fixed date just after epoch. Assert the overlays actually draw, too.
mkdir tle csv
for dir in tle csv; do
    printf '[earth]\nsatellite_file=%s/%s/iss\n' "$PWD" "$dir" > "$dir.conf"
    printf '25544 "" image=none altcirc=0 color=yellow trail={ground,-5,10,2} trail_output=%s.trail\n' \
        "$dir" > "$dir/iss"
done
cat > tle/iss.tle <<'TLE'
ISS (ZARYA)
1 25544U 98067A   04005.51955591  .00019728  00000-0  19085-3 0  9701
2 25544  51.6297  78.4979 0006431 344.6528 119.5204 15.66405360292643
TLE
head -n 1 tracked.omm.csv > csv/iss.omm.csv
cat >> csv/iss.omm.csv <<'CSV'
ISS (ZARYA),25544,2004-01-05T12:28:09.630624Z,15.66405360,0.0006431,51.6297,78.4979,344.6528,119.5204,0.00019085,0.00019728,0
CSV
date=20040105.124000
render base.conf fixed-base.png "$date"
render tle.conf fixed-tle.png "$date"
render csv.conf fixed-csv.png "$date"
cmp fixed-tle.png fixed-csv.png
[ "$(wc -l < tle.trail)" -gt 3 ]
cmp tle.trail csv.trail
if cmp -s fixed-base.png fixed-csv.png; then
    printf '%s\n' 'The fixed-date parity fixture did not actually render an overlay' >&2
    exit 1
fi

# Reproduce the audit with the bundled iss.tle available earlier/later
# in the lookup and a marker list with its own OMM companion.
printf '[earth]\nsatellite_file=iss\n' > search.conf
rm csv.trail
"$binary" -searchdir "$assets" -searchdir "$PWD/csv" -target earth \
    -projection rectangular -date "$date" -geometry 320x160 -num_times 1 \
    -config search.conf -output selected.png
cmp fixed-csv.png selected.png
cmp tle.trail csv.trail

# Old files with unknown options still render. These rows previously
# stopped at the first unrecognized key or reported the wrong problem.
render csv.conf clean.png "$date"
for options in 'colour=red' 'color=yellow bogus=1'; do
    printf '25544 "" image=none altcirc=0 color=yellow trail={ground,-5,10,2} %s\n' \
        "$options" > csv/iss
    render csv.conf forgiving.png "$date" 2> forgiving.log
    cmp clean.png forgiving.png
    [ ! -s forgiving.log ]
done

# The reader accepts the low orbit at epoch, but propagation with drag
# one day later yields an invalid position. Repeated rows warn only once.
head -n 1 tracked.omm.csv > failure.omm.csv
cat >> failure.omm.csv <<'CSV'
Nonpropagating,100058,2026-11-25T12:00:00Z,16.8,0.00045,51.64,27,115,245,0.00028,0.00011,0
CSV
printf '[earth]\nsatellite_file=%s/failure\n' "$PWD" > failure.conf
printf '100058 "First"\n100058 "Second"\n' > failure
render base.conf failure-base.png 20261126.120000
render failure.conf failure.png 20261126.120000 2> failure.log
cmp failure-base.png failure.png
[ "$(grep -c 'Cannot propagate OMM satellite 100058 from .*failure.omm.csv:' failure.log)" -eq 1 ]
printf '%s\n' 'OMM overlays, TLE parity and precedence, and propagation diagnostics passed'

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
        -date 20261125.120000 -geometry 320x160 -num_times 1 \
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
printf '%s\n' 'OMM six- and nine-digit overlays and TLE precedence passed'

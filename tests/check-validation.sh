#!/bin/sh
# Read-only renderer validation. The basic assertions need only POSIX tools.
set -eu
xplanet=$1
source=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-validate.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'exit 1' HUP INT TERM
mkdir "$work/home"
HOME="$work/home"
LC_ALL=C
export HOME LC_ALL
cd "$work"

check()
{
    expected=$1
    name=$2
    shift 2
    status=0
    "$xplanet" "$@" >"$name.json" 2>"$name.stderr" || status=$?
    if [ "$status" -ne "$expected" ]; then
        cat "$name.json" "$name.stderr" >&2
        echo "$name: expected status $expected, got $status" >&2
        exit 1
    fi
    if [ "$expected" -eq 0 ]; then valid=true; else valid=false; fi
    grep -q "^{" "$name.json"
    grep -q '"schema_version":1' "$name.json"
    grep -q "\"valid\":$valid" "$name.json"
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$name.json" "$valid" <<'PY'
import json
import sys
from pathlib import Path

p = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
assert set(p) == {"schema_version", "valid", "diagnostics"}
assert p["schema_version"] == 1 and p["valid"] is (sys.argv[2] == "true")
assert isinstance(p["diagnostics"], list)
assert p["valid"] is (not any(d["severity"] == "error" for d in p["diagnostics"]))
for d in p["diagnostics"]:
    assert isinstance(d["code"], str) and d["code"]
    assert isinstance(d["message"], str) and d["message"]
    if "line" in d:
        assert isinstance(d["line"], int) and d["line"] > 0
PY
    fi
}

cat >good.conf <<'CONF'
[earth]
map=none
CONF
check 0 good --validate -searchdir "$source/xplanet" -config good.conf \
    -projection rectangular -date 20261126.123456 -geometry 160x80
grep -q '"diagnostics":\[\]' good.json

# These options are recognized but cannot trigger rendering or writes.
printf 'keep this markerbounds file\n' >bounds
cp bounds expected
check 0 read_only -validate -searchdir "$source/xplanet" -config good.conf \
    -fork -output must-not-exist.png -num_times 1 -markerbounds bounds \
    -prev_command 'touch prev-ran' -post_command 'touch post-ran'
grep -q '"code":"skipped_hook"' read_only.json
[ ! -e must-not-exist.png ]
[ ! -e prev-ran ]
[ ! -e post-ran ]
cmp bounds expected

# An option value containing the selector is not a validation request.
"$xplanet" -label_string -validate -version >version.txt 2>version.stderr
grep -q '^Xplanet ' version.txt
if grep -Eq '^[[:space:]]+JPEG[[:space:]]*$' version.txt; then
    printf '[earth]\nmap=earth.jpg\nnight_map=night.jpg\n' >maps.conf
    check 0 maps -validate -searchdir "$source/xplanet" -config maps.conf
    grep -q '"diagnostics":\[\]' maps.json
    check 0 bundled_overlay -validate -searchdir "$source/xplanet" -config overlay_clouds
    grep -q '"severity":"warning","code":"missing_file".*"field":"earth.cloud_map"' bundled_overlay.json
fi
if grep -q '"schema_version"' version.txt; then
    echo "A label containing the validation selector started validation" >&2
    exit 1
fi

check 1 argument -validate -date 20260230.120000
grep -q '"code":"arguments"' argument.json
grep -q 'Invalid -date' argument.json
check 1 argument_number -validate -proj_param invalid
grep -q 'Invalid numeric value for -proj_param' argument_number.json
check 1 argument_unknown -validate -not-a-real-option
grep -q 'not-a-real-option' argument_unknown.json
check 1 argument_help -validate -help
grep -q '"code":"arguments"' argument_help.json

check 1 missing_config -validate -config absent.conf
grep -q '"path":"absent.conf"' missing_config.json

cat >bad.conf <<'CONF'
[earth]
shade=garbage
map=not-a-real-map.png
CONF
check 1 bad -validate -searchdir "$source/xplanet" -config bad.conf
grep -q '"path":"bad.conf","line":2' bad.json
grep -q '"field":"earth.map"' bad.json

# Check that secondary maps are actually decoded rather than just found.
printf 'not an image\n' >bad.png
cat >bad_image.conf <<'CONF'
[earth]
map=none
bump_map=bad.png
CONF
check 1 image -validate -searchdir "$source/xplanet" -config bad_image.conf
grep -q '"code":"invalid_image"' image.json
grep -q '"field":"earth.bump_map"' image.json

check 1 missing_marker -validate -config good.conf -searchdir "$source/xplanet" \
    -marker_file 'quoted"name.txt'
grep -q 'quoted\\"name.txt' missing_marker.json

printf '20261126.120000 10 0 0\nnot an observer record\n' >observer
check 0 origin -validate -searchdir "$source/xplanet" -config good.conf \
    -origin_file observer -interpolate_origin_file
grep -q '"severity":"warning","code":"resource".*"path":"observer","line":2' origin.json
printf '%s\n' 'not an observer record' > empty-observer
check 1 empty_origin -validate -config good.conf -origin_file empty-observer
grep -q '"severity":"error","code":"resource".*No usable records' empty_origin.json

printf '[earth]\nmap=none\n' >flat.conf
check 0 flat -validate -config flat.conf -body earth

# A configured NORAD row uses the selected orbit companion, without
# propagating the orbit or opening the row's optional trail output file.
cat >tracked.omm.csv <<'CSV'
OBJECT_NAME,NORAD_CAT_ID,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,RA_OF_ASC_NODE,ARG_OF_PERICENTER,MEAN_ANOMALY,BSTAR,MEAN_MOTION_DOT,MEAN_MOTION_DDOT
Example nine digits,987654321,2026-11-25T12:00:00.123456,14.89,0.00072,98,98,55,95,0.00019,0.00009,0
CSV
printf '[earth]\nmap=none\nsatellite_file=tracked\n' >satellites.conf
printf '987654321 "" image=none colour=red bogus=1 trail_output=trail-not-created\n' >tracked
check 0 satellite -validate -config satellites.conf
[ ! -e trail-not-created ]
grep -q '"severity":"info","code":"satellite_orbit_source".*"path":"tracked.omm.csv"' satellite.json
tail -n 1 tracked.omm.csv > duplicate-row
cat duplicate-row >> tracked.omm.csv
check 0 satellite_duplicate -validate -config satellites.conf
grep -q '"severity":"warning","code":"resource".*Duplicate NORAD_CAT_ID.*"line":3' satellite_duplicate.json

# The local marker's OMM CSV beats a bundled TLE with the same basename.
local_dir='local"catalog'
mkdir "$local_dir"
cp tracked "$local_dir/iss"
cp tracked.omm.csv "$local_dir/iss.omm.csv"
printf '[earth]\nmap=none\nsatellite_file=iss\n' > local.conf
check 0 local_companion -validate -config local.conf -searchdir "$source/xplanet" \
    -searchdir "$PWD/$local_dir"
grep '"code":"satellite_orbit_source"' local_companion.json | grep -Fq 'local\"catalog/iss.omm.csv'

# The same directory still prefers TLE, and unusable data is an error.
cp "$source/xplanet/satellites/iss.tle" "$local_dir/iss.tle"
printf '25544 "" image=none\n' > "$local_dir/iss"
check 0 same_directory -validate -config local.conf -searchdir "$source/xplanet" \
    -searchdir "$PWD/$local_dir"
grep '"code":"satellite_orbit_source"' same_directory.json | grep -Fq 'local\"catalog/iss.tle'
rm "$local_dir/iss.tle"
printf 'invalid TLE\n' > "$local_dir/iss.tle"
check 1 broken_companion -validate -config local.conf -searchdir "$source/xplanet" \
    -searchdir "$PWD/$local_dir"
grep -q '"severity":"error","code":"resource".*No usable TLE orbital records' broken_companion.json

printf '# Active satellites\n100057 "" image=none\n' >tracked
check 1 unmatched_satellite -validate -config satellites.conf
grep -q '"code":"unknown_satellite"' unmatched_satellite.json
grep -q '"path":"tracked","line":2' unmatched_satellite.json
grep -q '100057' unmatched_satellite.json

# Nonregular references must fail, not block waiting for a producer.
mkfifo marker-pipe
check 1 pipe -validate -config good.conf -marker_file marker-pipe
grep -q '"code":"missing_file"' pipe.json

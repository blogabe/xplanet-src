#!/bin/sh
# Keep tests independent of both the user's assets and the active desktop.
set -eu
binary=$1
assets=$2
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-check.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
mkdir "$scratch/home" "$scratch/work"
HOME="$scratch/home"
LC_ALL=C
export HOME LC_ALL
cd "$scratch/work"
"$binary" -version > version.log
grep -q '^TLE catalog IDs: Alpha-5$' version.log
if ! grep -Eq '^[[:space:]]+PNG[[:space:]]*$' version.log ||
   ! grep -Eq '^[[:space:]]+JPEG[[:space:]]*$' version.log; then
    printf '%s\n' 'SKIP: Earth PNG smoke test needs PNG and JPEG support'
    exit 0
fi
if ! "$binary" -searchdir "$assets" -target earth -projection rectangular \
    -date 20261125.120000 -geometry 320x160 -num_times 1 -verbosity 3 \
    -output earth.png > render.log 2>&1; then
    cat render.log >&2
    exit 1
fi
grep -F "Looking for $assets/images/earth.jpg ... found" render.log >/dev/null
header=$(od -An -tx1 -N24 earth.png | tr -d '[:space:]')
[ "$header" = 89504e470d0a1a0a0000000d4948445200000140000000a0 ]
printf '%s\n' 'Fixed-date Earth render passed (320x160 PNG)'

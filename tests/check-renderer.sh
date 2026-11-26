#!/bin/sh
# Keep tests independent of both the user's assets and the active desktop.
set -eu
binary=$1
assets=$2
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-check.XXXXXX")
live_pid=
trap 'if [ -n "$live_pid" ]; then kill "$live_pid" 2>/dev/null || :; wait "$live_pid" 2>/dev/null || :; fi
      rm -rf -- "$scratch"' EXIT
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

# Exercise the same CLI path for success, rejected destinations and I/O errors.
render()
{
    "$binary" -searchdir "$assets" -target earth -projection rectangular \
        -date 20261125.120000 -geometry 320x160 "$@"
}
fail_output()
{
    if render -num_times 1 -output "$1" > failure.log 2>&1; then
        printf 'Unexpected success for %s\n' "$1" >&2
        return 1
    fi
}
assert_no_temp()
{
    for temp in .xplanet-image.*; do
        if [ -e "$temp" ]; then
            printf 'Temporary output left behind: %s\n' "$temp" >&2
            return 1
        fi
    done
}

cp earth.png last-good.png
cp last-good.png unsupported.xyz
fail_output unsupported.xyz
cmp last-good.png unsupported.xyz
if ! grep -Eq '^[[:space:]]+PBM[[:space:]]*$' version.log; then
    cp last-good.png disabled.ppm
    fail_output disabled.ppm
    cmp last-good.png disabled.ppm
fi

mkdir directory.png
fail_output directory.png
[ -d directory.png ]
fail_output directory.png/
ln -s last-good.png symlink.png
fail_output symlink.png
[ -L symlink.png ]
cmp last-good.png earth.png
mkfifo pipe.png
fail_output pipe.png
[ -p pipe.png ]

mkdir dotted.parent
render -num_times 1 -output_map dotted.parent/map -output dotted.parent/no-suffix
[ -s dotted.parent/no-suffix.jpg ] && [ -s dotted.parent/map.jpg ]
(umask 027; render -num_times 1 -output new.png)
[ "$(LC_ALL=C ls -ld new.png | cut -c 1-10)" = '-rw-r-----' ]
render -num_times 2 -wait 0 -output_start_index 9 -output frame.png
[ -s frame09.png ] && [ -s frame10.png ]

formats='bmp jpg png'
grep -Eq '^[[:space:]]+GIF[[:space:]]*$' version.log && formats="$formats gif"
grep -Eq '^[[:space:]]+TIFF[[:space:]]*$' version.log && formats="$formats tiff"
grep -Eq '^[[:space:]]+PBM[[:space:]]*$' version.log && formats="$formats pbm pgm ppm"
for ext in $formats; do
    # Preserve existing permission bits, and leave another hard link alone.
    printf 'old image\n' > "output.$ext"
    chmod 640 "output.$ext"
    ln "output.$ext" "old.$ext"
    render -num_times 1 -output "output.$ext"
    [ "$(LC_ALL=C ls -ld "output.$ext" | cut -c 1-10)" = '-rw-r-----' ]
    if cmp -s "old.$ext" "output.$ext"; then
        printf 'Successful render did not replace %s\n' "$ext" >&2
        exit 1
    fi
    [ "$(cat "old.$ext")" = 'old image' ]
    cp "output.$ext" "last.$ext"
    # Force a real write failure against an ordinary file in the scratch dir.
    # Ignore SIGXFSZ so the encoder has to handle the underlying I/O error.
    if (
        trap '' XFSZ && ulimit -f 1 || exit 125
        render -num_times 1 -output "output.$ext"
    ) >/dev/null 2>&1; then
        printf 'Unexpected success at the file-size limit: %s\n' "$ext" >&2
        exit 1
    else
        result=$?
        [ "$result" -eq 1 ] || exit "$result"
    fi
    cmp "last.$ext" "output.$ext"
    assert_no_temp
done
printf '%s\n' "Output preservation passed ($formats)"

# A missing explicitly configured input must not replace a published frame.
printf '[earth]\nmap=%s/working-map.jpg\n' "$PWD" > refresh.conf
cp last-good.png finite.png
if render -config refresh.conf -num_times 1 -output finite.png > finite.log 2>&1; then
    printf '%s\n' 'A finite render unexpectedly succeeded without its map' >&2
    exit 1
fi
grep -F 'frame not published' finite.log >/dev/null
cmp last-good.png finite.png

printf 'previous frame\n' > live.png
cp live.png old-frame
"$binary" -searchdir "$assets" -target earth -projection rectangular \
    -date 20261125.120000 -geometry 320x160 -config refresh.conf \
    -wait 1 -output live.png > live.log 2>&1 &
live_pid=$!
for attempt in 1 2 3 4 5 6; do
    if grep -F 'frame not published' live.log >/dev/null; then break; fi
    kill -0 "$live_pid"
    sleep 1
done
grep -F 'frame not published' live.log >/dev/null
cmp old-frame live.png
ln -s "$assets/images/earth.jpg" working-map.jpg
for attempt in 1 2 3 4 5 6; do
    if ! cmp -s old-frame live.png; then break; fi
    kill -0 "$live_pid"
    sleep 1
done
[ "$(od -An -tx1 -N8 live.png | tr -d '[:space:]')" = 89504e470d0a1a0a ]
kill "$live_pid"
wait "$live_pid" 2>/dev/null || :
live_pid=
printf '%s\n' 'Required map failure and refresh recovery passed'

TZ=Pacific/Honolulu render -label -utclabel -date_format '%Y %Z' \
    -num_times 2 -wait 0 -output tz.png \
    -post_command 'printf "%s\n" "$TZ" >> tz.log'
printf '%s\n' Pacific/Honolulu Pacific/Honolulu > expected-tz
cmp expected-tz tz.log
printf '%s\n' 'Label timezone restoration passed'

"$binary" -help > help.log
grep -F 'Valid options' help.log >/dev/null
for flag in -not-a-real-xplanet-option -XYZ_file; do
    if "$binary" "$flag" unused > cli-error.log 2>&1; then exit 1; fi
done
if render -date 20260230 -num_times 1 -output invalid-date.png > cli-error.log 2>&1; then exit 1; fi
[ ! -e invalid-date.png ]
render -date 20261125 -num_times 1 -output .midnight
render -date 20261125.000000 -num_times 1 -output explicit-midnight.jpg
cmp .midnight.jpg explicit-midnight.jpg
render -num_times 1 -transpng transparent
[ "$(od -An -tx1 -j25 -N1 transparent.png | tr -d '[:space:]')" = 06 ]
if render -num_times 1 -transpng wrong.jpg > cli-error.log 2>&1; then exit 1; fi
[ ! -e wrong.jpg ]

printf '#%4100s\n[earth]\nmap=absent-after-long-line.jpg\n' ' ' > long.conf
if render -config long.conf -num_times 1 -output long.png > long.log 2>&1; then exit 1; fi
grep -F 'ignoring an overlong configuration line' long.log >/dev/null
grep -F 'absent-after-long-line.jpg' long.log >/dev/null

printf '  # position not needed in a timed sequence\n\n20261125.120000\n' > dates.txt
render -origin_file "$PWD/dates.txt" -num_times 1 -output origin-date.png
[ "$(od -An -tx1 -N8 origin-date.png | tr -d '[:space:]')" = 89504e470d0a1a0a ]
printf '  # no records\n\n' > empty-origin.txt
if render -origin_file "$PWD/empty-origin.txt" -output no-origin.png > origin-error.log 2>&1; then exit 1; fi
grep -F 'No usable records' origin-error.log >/dev/null
printf '20261125.120000 10 20 30\n' > positioned.txt
render -origin_file "$PWD/positioned.txt" -interpolate_origin_file \
    -num_times 1 -output origin-endpoint.png
[ -s origin-endpoint.png ]
printf '[earth]\nmagnify=not-a-number\nshade=bad\n' > numbers.conf
render -config numbers.conf -num_times 1 -output numbers.png > numbers.log 2>&1
grep -F 'numbers.conf:2: invalid magnify' numbers.log >/dev/null
grep -F 'numbers.conf:3: invalid shade' numbers.log >/dev/null
printf '%s\n' 'CLI, configuration and origin input checks passed'

"$binary" --version > long-option.log
grep -F 'TLE catalog IDs: Alpha-5' long-option.log >/dev/null
render -verbo 0 -num_times 1 -output end-of-options.png --
[ -s end-of-options.png ]
if "$binary" -ver > ambiguous.log 2>&1; then exit 1; fi
printf '%s\n' 'CLI option spelling and ordering passed'

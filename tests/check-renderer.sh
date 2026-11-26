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

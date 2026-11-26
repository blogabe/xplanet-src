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
# Preserve the pre-1.6 abbreviation for -wait on all builds.
render -wa 300 -num_times 1 -output wait-short.png
cmp earth.png wait-short.png

# Backend selection must not publish a desktop image when -output is used.
if grep -Fq 'Wallpaper backends: appkit, finder' version.log; then
    for backend in appkit finder; do
        render -wallpaper_backend "$backend" -num_times 1 -output "$backend.png"
        cmp earth.png "$backend.png"
    done
    # A desktop test is safe here only because the invalid pair must be
    # rejected before any child process or AppKit call can occur.
    if render -wallpaper_backend appkit -fork -verbosity 2 -num_times 1 \
        -tmpdir "$scratch/does-not-exist" > fork.log 2>&1; then
        echo 'AppKit accepted fork-without-exec' >&2
        exit 1
    fi
    grep -Fq -- '-fork is not supported with AppKit wallpaper' fork.log
    grep -Fq -- '-wallpaper_backend finder' fork.log

    # With -output, the parent exits before the detached child writes its PNG.
    render -wallpaper_backend appkit -fork -num_times 1 -output fork-output.png
    attempts=0
    while [ ! -f fork-output.png ] && [ "$attempts" -lt 10 ]; do
        attempts=$((attempts + 1))
        sleep 1
    done
    cmp earth.png fork-output.png
    expected='expects appkit or finder'
else
    expected='requires a macOS wallpaper build'
fi
if render -wallpaper_backend invalid -num_times 1 -output backend-invalid.png > backend.log 2>&1; then
    echo 'Invalid wallpaper backend was accepted' >&2
    exit 1
fi
grep -Fq "$expected" backend.log
[ ! -e backend-invalid.png ]

# Lossless, uniform 8x4 RGB(40,70,120) PNG. Render with no day/night
# attenuation and check every output BGR sample. Legacy integer filtering
# and shading truncate intermediate values; allow up to five levels per channel.
printf '%b' '\0211\0120\0116\0107\0015\0012\0032\0012\0000\0000\0000\0015\0111\0110\0104\0122' \
    '\0000\0000\0000\0010\0000\0000\0000\0004\0010\0002\0000\0000\0000\0074\0257\0351\0247' \
    '\0000\0000\0000\0021\0111\0104\0101\0124\0170\0234\0143\0320\0160\0253\0300\0212\0030\0250' \
    '\0047\0001\0000\0224\0057\0034\0301\0136\0034\0235\0341\0000\0000\0000\0000\0111\0105\0116' \
    '\0104\0256\0102\0140\0202' > solid.png
printf '[earth]\nmap=%s/solid.png\nnight_map=%s/solid.png\nshade=100\n' "$PWD" "$PWD" > solid.conf
render -config solid.conf -num_times 1 -output solid.bmp
od -An -v -tu1 -j54 solid.bmp | awk '
    BEGIN { split("120 70 40", bgr, " ") }
    {
        for (i = 1; i <= NF; ++i) {
            delta = $i - bgr[n++ % 3 + 1]
            if (delta < -5 || delta > 5) exit 1
        }
    }
    END { if (n != 320 * 160 * 3) exit 1 }
'
printf '%s\n' 'Fixed-date render matches all expected RGB pixels'

# Lossless 16x8 RGB PNG fixtures. The two latitude bands and four longitude
# bands have eight distinct colors; the other texture is black. No image
# conversion utility is needed at test time.
printf '%b' '\0211\0120\0116\0107\0015\0012\0032\0012\0000\0000\0000\0015\0111\0110\0104\0122' \
    '\0000\0000\0000\0020\0000\0000\0000\0010\0010\0002\0000\0000\0000\0177\0024\0350' \
    '\0300\0000\0000\0000\0064\0111\0104\0101\0124\0170\0332\0143\0120\0132\0270\0027' \
    '\0216\0226\0032\0124\0303\0121\0301\0036\0073\0070\0332\0023\0335\0007\0107\0014' \
    '\0203\0120\0303\0042\0043\0031\0070\0122\0231\0146\0017\0107\0136\0332\0353\0340' \
    '\0150\0127\0253\0016\0034\0015\0102\0015\0000\0176\0242\0245\0141\0370\0132\0171' \
    '\0056\0000\0000\0000\0000\0111\0105\0116\0104\0256\0102\0140\0202' > asymmetric.png
printf '%b' '\0211\0120\0116\0107\0015\0012\0032\0012\0000\0000\0000\0015\0111\0110\0104\0122' \
    '\0000\0000\0000\0020\0000\0000\0000\0010\0010\0002\0000\0000\0000\0177\0024\0350' \
    '\0300\0000\0000\0000\0016\0111\0104\0101\0124\0170\0332\0143\0140\0030\0005\0203' \
    '\0001\0000\0000\0001\0210\0000\0001\0357\0011\0342\0047\0000\0000\0000\0000\0111' \
    '\0105\0116\0104\0256\0102\0140\0202' > dark.png

render_map()
{
    date=$1 config=$2 output=$3
    shift 3
    "$binary" -searchdir "$assets" -target earth \
        -geometry 320x160 -date "$date" -config "$config" \
        -num_times 1 -output "$output" "$@"
}

# The output encoder writes 24-bit BMP, padded per row (320*3 has no padding).
# x/y are measured from the image top-left, not BMP's bottom-up byte order.
assert_rgb()
{
    image=$1 x=$2 y=$3 r=$4 g=$5 b=$6 tolerance=${7:-5}
    offset=$((54 + (159 - y) * 320 * 3 + x * 3))
    if ! od -An -v -tu1 -j "$offset" -N3 "$image" | awk -v r="$r" -v g="$g" -v b="$b" -v tolerance="$tolerance" '
        function near(actual, expected) { return actual >= expected-tolerance && actual <= expected+tolerance }
        NF { ok = NF == 3 && near($1,b) && near($2,g) && near($3,r); ++rows }
        END { exit !(rows == 1 && ok) }
    '; then
        printf 'Unexpected %s pixel at (%s,%s): expected RGB %s %s %s; got BGR ' \
            "$image" "$x" "$y" "$r" "$g" "$b" >&2
        od -An -v -tu1 -j "$offset" -N3 "$image" >&2
        return 1
    fi
}

# Longitude centers -135/-45/45/135, and latitude centers +45/-45.
# Explicit expected positions are independent of the rendered pixels.
printf '[earth]\nmap=%s/asymmetric.png\nnight_map=%s/asymmetric.png\nshade=100\n' \
    "$PWD" "$PWD" > asymmetric.conf
render_map 20260320.120000 asymmetric.conf oriented.bmp -projection rectangular -longitude 0 -latitude 0
assert_rgb oriented.bmp  40  40  34 161 189
assert_rgb oriented.bmp 120  40 165  48 123
assert_rgb oriented.bmp 200  40 112 188  62
assert_rgb oriented.bmp 280  40 188  91 142
assert_rgb oriented.bmp  40 120 162  50  28
assert_rgb oriented.bmp 120 120  36 150  63
assert_rgb oriented.bmp 200 120  74  43 174
assert_rgb oriented.bmp 280 120 186 133  44
printf '%s\n' 'Rectangular render preserves eight latitude/longitude color samples'

# At ±45°, cylindrical equal-area (Lambert) places the same texture bands
# at y=80*(1∓sin(45°)), rather than the rectangular rows 40 and 120.
render_map 20260320.120000 asymmetric.conf lambert.bmp \
    -projection lambert -longitude 0 -latitude 0
assert_rgb lambert.bmp 120  23 165 48 123
assert_rgb lambert.bmp 200 137  74 43 174

# No projection option: test the globe, centered on the 45°E texture band.
# On its central meridian points above and below the equator must land in
# opposite hemispheres. A 90%-radius disk leaves the corner as background.
# The globe's texture filtering needs a wider tolerance than the flat maps.
render_map 20260320.120000 asymmetric.conf globe.bmp \
    -longitude 45 -latitude 0 -radius 90
assert_rgb globe.bmp 160  29 112 188 62 24
assert_rgb globe.bmp 160 131  74  43 174 24
assert_rgb globe.bmp  10  10   0   0   0
printf '%s\n' 'Lambert and default globe preserve latitude orientation'

# Catalog stars must change the globe background with random stars disabled.
# No explicit projection: a full-map projection has no surrounding sky.
printf '# No catalog sources\n' > empty-stars
render_map 20260320.120000 asymmetric.conf empty-sky.bmp \
    -longitude 45 -latitude 0 -range 8 -radius 24 -starfreq 0 -starmap "$PWD/empty-stars"
render_map 20260320.120000 asymmetric.conf catalog-sky.bmp \
    -longitude 45 -latitude 0 -range 8 -radius 24 -starfreq 0 -starmap BSC -verbosity 3 \
    > catalog.log 2>&1
grep -F "Looking for $assets/stars/BSC ... found" catalog.log >/dev/null
# Count lit pixels in the top 24 rows, all outside the 39-pixel-radius globe.
# Require stars outside the globe, not simply any difference in the frame.
sky_pixels()
{
    od -An -v -tu1 -j "$((54 + 136 * 960))" -N "$((24 * 960))" "$1" | awk '
        { for (i = 1; i <= NF; ++i) if ($i > 0) ++lit }
        END { print lit+0 }
    '
}
[ "$(sky_pixels catalog-sky.bmp)" -gt "$(sky_pixels empty-sky.bmp)" ]
if grep -Eq '^[[:space:]]+FreeType' version.log; then
    render_map 20260320.120000 asymmetric.conf catalog-label.bmp \
        -longitude 45 -latitude 0 -range 8 -radius 24 -starfreq 0 -starmap BSC \
        -label -font FreeMonoBold.ttf -verbosity 3 > catalog-label.log 2>&1
    grep -F "Looking for $assets/fonts/FreeMonoBold.ttf ... found" catalog-label.log >/dev/null
    if cmp -s catalog-sky.bmp catalog-label.bmp; then exit 1; fi
fi
printf '%s\n' 'Catalog stars render outside the globe; bundled font resolves with FreeType'

# Near equinox, -45° longitude is lit at noon UTC and +135° at midnight UTC.
# Both samples are far from the terminator, so expect the exact lit or night
# texture (within the same tolerance), not a renderer-generated golden image.
printf '[earth]\nmap=%s/asymmetric.png\nnight_map=%s/dark.png\nshade=0\n' \
    "$PWD" "$PWD" > lighting.conf
render_map 20260320.120000 lighting.conf noon.bmp -projection rectangular -longitude 0 -latitude 0
render_map 20260320.000000 lighting.conf midnight.bmp -projection rectangular -longitude 0 -latitude 0
assert_rgb noon.bmp     120 40 165 48 123
assert_rgb noon.bmp     280 40   0  0   0
assert_rgb midnight.bmp 120 40   0  0   0
assert_rgb midnight.bmp 280 40 188 91 142
printf '%s\n' 'Fixed dates swap the expected day and night samples'

# Expand the verified uniform texture to full resolution so interpolation
# across the terminator does not blur the reference locations.
render -config solid.conf -num_times 1 -output expanded-solid.png

# Probe real day/night boundaries rather than only their interiors. Around
# equinox, 15°W on the equator crosses dawn between 06:00 and 08:00 UTC.
# At 60°N, 102°E at noon UTC is dark at the March equinox but sunlit
# a month later as northern solar declination increases.
printf '[earth]\nmap=%s/expanded-solid.png\nshade=0\ntwilight=0\n' \
    "$PWD" > terminator.conf
render_map 20260320.060000 terminator.conf dawn-before.bmp -projection rectangular -longitude 0 -latitude 0
render_map 20260320.080000 terminator.conf dawn-after.bmp -projection rectangular -longitude 0 -latitude 0
render_map 20260320.120000 terminator.conf march.bmp -projection rectangular -longitude 0 -latitude 0
render_map 20260419.120000 terminator.conf april.bmp -projection rectangular -longitude 0 -latitude 0
assert_rgb dawn-before.bmp 147 80 0 0 0
assert_rgb march.bmp 251 27 0 0 0
assert_rgb dawn-after.bmp 147 80 40 70 120
assert_rgb april.bmp 251 27 40 70 120
printf '%s\n' 'Equatorial dawn and high-latitude twilight follow the UTC date'

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
for shade in 0 50 50.0; do
    printf '[earth]\nshade=%s\n' "$shade" > shade.conf
    render -config shade.conf -num_times 1 -output "shade-$shade.png" > shade.log 2>&1
    if grep -F 'invalid shade' shade.log; then exit 1; fi
done
cmp shade-50.png shade-50.0.png
if cmp -s shade-0.png shade-50.png; then
    printf '%s\n' 'Shade fixture did not affect the image' >&2
    exit 1
fi
# Scalar integer settings accept exact integral decimals, not conversion
# rounded to an integer. Invalid overrides must leave the valid setting intact.
for variant in integer decimal fraction tiny-fraction overflow underflow junk; do
    printf '[earth]\ngrid=true\n' > "integer-$variant.conf"
    for setting in bump_shade=30 cloud_threshold=20 grid1=20 grid2=10 \
        marker_fontsize=12 arc_thickness=3 twilight=15; do
        value=${setting#*=}
        case $variant in
            integer) valid=$value ;;
            decimal) valid=$value.000 ;;
            *) valid=$value ;;
        esac
        key=${setting%%=*}
        printf '%s=%s\n' "$key" "$valid" >> "integer-$variant.conf"
        case $variant in
            fraction) invalid=$value.5 ;;
            tiny-fraction) invalid=$value.000000000000000000000000000001 ;;
            overflow) invalid=2147483648.0 ;;
            underflow) invalid=-2147483649.0 ;;
            junk) invalid=$value.0x ;;
            *) continue ;;
        esac
        printf '%s=%s\n' "$key" "$invalid" >> "integer-$variant.conf"
    done
    render -config "integer-$variant.conf" -num_times 1 \
        -output "integer-$variant.png" > "integer-$variant.log" 2>&1
    case $variant in
        integer|decimal)
            if grep -F ': invalid ' "integer-$variant.log"; then exit 1; fi ;;
        *)
            line=4
            for key in bump_shade cloud_threshold grid1 grid2 marker_fontsize thickness twilight; do
                grep -F "integer-$variant.conf:$line: invalid $key value; keeping the previous value." \
                    "integer-$variant.log" >/dev/null
                line=$((line + 2))
            done ;;
    esac
    cmp integer-integer.png "integer-$variant.png"
done
printf '%s\n' 'CLI, configuration and origin input checks passed'

"$binary" --version > long-option.log
grep -F 'TLE catalog IDs: Alpha-5' long-option.log >/dev/null
render -verbo 0 -num_times 1 -output end-of-options.png --
[ -s end-of-options.png ]
if "$binary" -ver > ambiguous.log 2>&1; then exit 1; fi
printf '%s\n' 'CLI option spelling and ordering passed'

# A malformed line must not discard the subsequent annotation records.
mkdir overlays
printf '0 0 "Recovered marker" color=red\n' > overlays/clean-markers
printf '0 -60 0 60 color=magenta\n' > overlays/clean-arcs
printf '25544 "Recovered ISS" color=red image=none altcirc=0\n' > overlays/clean-sat
sed -n '1,3p' "$assets/satellites/iss.tle" > overlays/clean-sat.tle
for kind in markers arcs sat; do
    { printf '%700s\n' bad; cat "overlays/clean-$kind"; } > "overlays/recover-$kind"
done
# Interleave a damaged set and an orphan element line before a complete set.
{
    sed -n '1,2p' overlays/clean-sat.tle
    printf '%700s\n' bad
    sed -n '3p' overlays/clean-sat.tle
    cat overlays/clean-sat.tle
} > overlays/recover-sat.tle
for kind in marker arc satellite; do
    case "$kind" in
        marker) suffix=markers ;;
        arc) suffix=arcs ;;
        satellite) suffix=sat ;;
    esac
    for variant in clean recover; do
        printf '[earth]\n%s_file=%s/overlays/%s-%s\n' "$kind" "$PWD" \
            "$variant" "$suffix" > overlays.conf
        render -config overlays.conf -num_times 1 -output "$variant-$kind.png" \
            > "$variant-$kind.log" 2>&1
    done
    grep -F ': ignoring an overlong record.' "recover-$kind.log" >/dev/null
    cmp "clean-$kind.png" "recover-$kind.png"
    if cmp -s earth.png "clean-$kind.png"; then
        printf 'Annotation fixture did not affect the image: %s\n' "$kind" >&2
        exit 1
    fi
done
grep -F 'recover-sat.tle:3: ignoring an overlong record.' recover-satellite.log >/dev/null
printf '%s\n' 'Annotation and orbital record recovery passed'

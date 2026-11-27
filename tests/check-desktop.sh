#!/bin/sh
# Native packaging and the fileless transport must be testable without a GUI.
set -eu
renderer=$1
helper=$2
assets=$3
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-pipe.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
umask 077
mkdir "$scratch/home" "$scratch/scene"
export HOME="$scratch/home" LC_ALL=C
cd "$scratch/scene"

"$renderer" -version > "$scratch/version"
if ! grep -qx 'Frame pipe: 1' "$scratch/version"; then
    [ ! -e "$helper" ] || { echo 'stale helper without renderer capability' >&2; exit 1; }
    if "$renderer" --frame-pipe 160x80 > "$scratch/no-support" 2>&1; then
        echo 'frame pipe mode accepted without support' >&2
        exit 1
    fi
    exit 0
fi
[ -x "$helper" ]
"$helper" --help | grep -q '^Usage: xplanet-desktop'
if ! grep -q '^[[:space:]]*JPEG$' "$scratch/version"; then
    echo 'SKIP: desktop Earth fixture needs JPEG support'
    exit 0
fi

set -- -searchdir "$assets" -target earth -projection rectangular -date 20261125.120000
"$renderer" "$@" -geometry 160x80 -num_times 1 -output "$scratch/reference.png"
"$renderer" --frame-pipe 160x80 "$@" -verbosity 3 > "$scratch/frame.png" 2> "$scratch/diagnostics"
cmp "$scratch/reference.png" "$scratch/frame.png"
"$renderer" --frame-pipe 160x80 "$@" -timewarp 0 > "$scratch/fixed-scene.png"
cmp "$scratch/reference.png" "$scratch/fixed-scene.png"
grep -q 'found' "$scratch/diagnostics"
[ -z "$(ls -A)" ]

# Offsets select regions of the full canvas rather than stretching the sub-image.
"$renderer" --frame-pipe 160x80 "$@" -geometry 80x40+10+15 > "$scratch/top-left.png"
"$renderer" --frame-pipe 160x80 "$@" -geometry 80x40-10-15 > "$scratch/bottom-right.png"
! cmp -s "$scratch/top-left.png" "$scratch/bottom-right.png"
for f in "$scratch/top-left.png" "$scratch/bottom-right.png"; do
    [ "$(od -An -tx1 -N24 "$f" | tr -d '[:space:]')" = 89504e470d0a1a0a0000000d49484452000000a000000050 ]
done
[ -z "$(ls -A)" ]

# Streaming cannot also write files, spawn delegated work or redirect wallpaper.
for option in '-output=not-created.png' '-geometry=99999x99999' '-num_times=2'               '-prev_command=touch not-created' '-save_desktop_file' '-timewarp=2' '-fork'; do
    if "$renderer" --frame-pipe 160x80 "$@" "$option" > "$scratch/none" 2> "$scratch/rejection"; then
        echo "unexpectedly accepted desktop option $option" >&2
        exit 1
    fi
    [ ! -s "$scratch/none" ]
    [ -s "$scratch/rejection" ]
done
[ -z "$(ls -A)" ]

# Use the installed helper's actual reader/worker/decoder, not a test parser.
"$helper" --check-frame 160x80 "$renderer" "$PWD" 1 5 1 -- "$@" > "$scratch/hash"
[ "$(wc -c < "$scratch/hash" | tr -d ' ')" = 65 ]
failure()
{
    expected=$1
    body=$2
    printf '#!/bin/sh\n%s\n' "$body" > "$scratch/worker"
    chmod 700 "$scratch/worker"
    if "$helper" --check-frame 160x80 "$scratch/worker" "$PWD" 1 0.25 1 -- > "$scratch/fault-out" 2> "$scratch/fault-log"; then
        echo "test frame incorrectly accepted: $expected" >&2
        exit 1
    fi
    [ ! -s "$scratch/fault-out" ]
    grep -Fq "$expected" "$scratch/fault-log"
}
failure 'no fresh frame' 'exit 0'
failure 'incomplete or invalid PNG' 'printf garbage'
failure 'renderer terminated (4)' 'exit 4'
failure 'renderer timed out' 'exec /bin/sleep 10'
# A renderer that emits too many bytes is stopped even if no timeout elapsed.
printf '#!/bin/sh\nexec /bin/dd if=/dev/zero bs=1048576 count=65\n' > "$scratch/worker"
if "$helper" --check-frame 160x80 "$scratch/worker" "$PWD" 1 10 1 -- > "$scratch/fault-out" 2> "$scratch/fault-log"; then
    echo 'oversized stream accepted' >&2
    exit 1
fi
grep -Fq 'frame pipe exceeded byte limit' "$scratch/fault-log"
[ -z "$(ls -A)" ]
printf '%s\n' 'Fileless desktop geometry, protocol and worker failure checks passed'

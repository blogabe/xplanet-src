#!/bin/sh
# Reproduce the renderer CI lanes without installing Xplanet.
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/xplanet-ci.XXXXXX")
trap 'rm -rf -- "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
export LC_ALL=C
set -- "${1:?usage: ci-build.sh linux|macos|macos-cspice}" \
    --disable-dependency-tracking "--prefix=$scratch/install" \
    --without-cygwin --without-x --without-xscreensaver \
    --with-gif --with-jpeg --with-png --with-tiff \
    --without-pango --without-pnm
profile=$1
shift
case "$profile" in
    linux)
        test "$(uname -s)" = Linux
        set -- "$@" --without-aqua --without-cspice
        ;;
    macos|macos-cspice)
        test "$(uname -s):$(uname -m)" = Darwin:arm64
        packages='freetype giflib jpeg-turbo libpng libtiff'
        if [ "$profile" = macos-cspice ]; then
            packages="$packages cspice"
            set -- "$@" --with-aqua --with-cspice
        else
            set -- "$@" --with-aqua --without-cspice
        fi
        for package in $packages; do
            prefix=$(brew --prefix "$package")
            CPPFLAGS="${CPPFLAGS:-} -I$prefix/include"
            LDFLAGS="${LDFLAGS:-} -L$prefix/lib"
            PKG_CONFIG_PATH="$prefix/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
        done
        export CPPFLAGS LDFLAGS PKG_CONFIG_PATH
        ;;
    *) printf '%s\n' "Unknown build profile: $profile" >&2; exit 2 ;;
esac

"$src/prepare-build.sh"
cd "$scratch"
if ! "$src/configure" "$@"; then
    cat config.log >&2
    exit 1
fi
make -j "${JOBS:-2}"
src/xplanet -version > features.txt
cat features.txt
for feature in GIF JPEG PNG TIFF FreeType; do
    grep -Eiq "^[[:space:]]+$feature([[:space:]]|$)" features.txt
done
if [ "$profile" = macos-cspice ]; then
    grep -Eq '^[[:space:]]+CSPICE_' features.txt
fi
make check

if [ "$profile" = macos-cspice ]; then
    sh "$src/tests/check-cspice.sh" "$PWD/src/xplanet" "$src/xplanet" "$(brew --prefix cspice)"
fi

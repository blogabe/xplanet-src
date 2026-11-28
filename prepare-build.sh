#!/bin/sh
# Preserve the shipped Autotools output after Git/archive timestamp changes.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for file in configure configure.ac acinclude.m4 aclocal.m4 config.h.in Makefile.in; do
    test -f "$source_dir/$file" || { echo "Missing source file: $file" >&2; exit 1; }
done
touch "$source_dir/aclocal.m4"
touch "$source_dir/configure" "$source_dir/config.h.in"
touch "$source_dir/Makefile.in"
find "$source_dir/src" -type f -name Makefile.in -exec touch {} +
printf '%s\n' 'Generated build-file timestamps prepared; no source contents changed.'

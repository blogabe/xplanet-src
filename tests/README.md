# Renderer regression checks

After configuring and building Xplanet, run from the source directory:

```sh
tmp=$(mktemp -d)
cc -Isrc/libsgp4sdp4 tests/tle_catalog.c \
  src/libsgp4sdp4/libsgp4sdp4.a -lm -o "$tmp/tle-catalog-test"
"$tmp/tle-catalog-test"
src/xplanet -version
```

The fixture covers legacy and Alpha-5 catalog IDs, including the skipped
letters and agreement between both TLE lines. The executable should
advertise `TLE catalog IDs: Alpha-5` so clients can detect support.

When building from a checkout or repacked source, run `./prepare-build.sh`
before `./configure` if timestamps would cause Make to regenerate the
shipped Autotools files. It changes timestamps only. Developers who edit
the build macros should also keep their generated counterparts in sync.

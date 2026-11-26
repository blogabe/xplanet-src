# Renderer regression checks

After configuring and building Xplanet, run `make check` from the build
directory. This tests five-digit and Alpha-5 satellite catalog IDs and verifies
that the executable advertises Alpha-5. When PNG and JPEG are enabled, it
also renders a fixed-date Earth scene using only the packaged data. It never
changes the desktop or uses the current user's maps.

When building from a checkout or repacked source, run `./prepare-build.sh`
before `./configure` if timestamps would cause Make to regenerate the
shipped Autotools files. It changes timestamps only. Keep the generated
Makefile in sync when editing the build rules.

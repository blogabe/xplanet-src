# Renderer regression checks

After configuring and building Xplanet, run `make check` from the build
directory. This tests five-digit and Alpha-5 satellite catalog IDs and verifies
that the executable advertises Alpha-5. A separate native check covers map
downsampling, including square, rectangular, odd-sized and one-row maps.
When PNG and JPEG are enabled, the CLI check
also renders a fixed-date Earth scene using only the packaged data. It never
changes the desktop or uses the current user's maps. The file-output checks
cover replacement, preservation of the old image after an I/O error, temporary
file cleanup, rejected destinations and frame naming. Each enabled writer is
tested by limiting the file size in a child process.

When building from a checkout or repacked source, run `./prepare-build.sh`
before `./configure` if timestamps would cause Make to regenerate the
shipped Autotools files. It changes timestamps only. Keep the generated
Makefile in sync when editing the build rules.

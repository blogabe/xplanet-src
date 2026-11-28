# Renderer CI

The workflow runs on pull requests and pushes to `main`. It builds headless
Linux and Apple silicon macOS with the tap defaults, plus a macOS CSPICE
build. The CSPICE lane uses synthetic kernels to check Earth coordinates in
AU, day/night geometry and placement of a spacecraft marker. It does not
download mission data or change the wallpaper.

With build dependencies installed, run `sh tests/ci-build.sh linux`,
`sh tests/ci-build.sh macos`, or `sh tests/ci-build.sh macos-cspice` from a
Git checkout. The CSPICE lane also needs Python 3. The scripts are for
repository CI and are not part of the source release archive.

Run `make distcheck` and a sanitizer build before tagging. Hosted CI does
not replace live macOS wallpaper or native Intel desktop testing.

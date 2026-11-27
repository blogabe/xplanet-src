# Xplanet

Xplanet is a command-line renderer for Earth, the planets and their moons.
It combines surface maps with sunlight and shadows, and can draw labels,
markers, satellite tracks and other overlays from local files. Use it to make
an image for a report, render a sequence of frames or set your Mac wallpaper.

This repository builds the `xplanet` binary. The separate
[macOS setup and automation project](https://github.com/blogabe/Xplanet)
provides the wallpaper service, TotalMarker data updates and the Configurator.

## Install on macOS

With Homebrew installed, use the [homebrew-xplanet tap](https://github.com/blogabe/homebrew-xplanet):

```sh
brew tap blogabe/xplanet
brew install --build-from-source blogabe/xplanet/xplanet
xplanet -version
```

The fully qualified formula name selects this tap instead of Homebrew's core
formula. The tap builds the renderer from a packaged source archive, so
Subversion is not needed. It currently installs the maintained `1.3.1.225`
build. The 1.6.0 development work described here is not available through
the tap.

## What's new in 1.6.0

AppKit is the default macOS wallpaper backend. It doesn't need Finder
Automation permission, and the Finder backend remains available explicitly.
Finite wallpaper runs now report a failed publish with a nonzero exit status.

## macOS wallpaper

On macOS, Xplanet 1.6 defaults to AppKit for wallpaper in a logged-in desktop
session. The new backend has been tested on Apple silicon; Intel compatibility
has not been verified. AppKit does not send Finder Automation requests. Use
`-wallpaper_backend finder` for an explicit rollback to the legacy backend. If
AppKit cannot publish a frame, Xplanet reports the error without switching to
Finder. A long-running wallpaper service retries on its regular schedule. For
AppKit wallpaper, run in the foreground or use a user LaunchAgent, not `-fork`.
To keep a legacy Finder command using `-fork`, explicitly choose
`-wallpaper_backend finder`. This does not change `-output` rendering.
The current implementation updates the main display only.

## Render an image

With Xplanet installed, run:

```sh
xplanet -target earth -projection rectangular -geometry 1200x600 \
  -num_times 1 -output earth.png
```

This writes `earth.png` in the current directory using the bundled Earth map.
Add `-date 20261125.120000` to render at noon UTC on November 25, 2026. A date
without a time means midnight UTC. Omit `-projection` to render a 3D globe.
For bodies with no bundled texture, supply your own map. See `man xplanet` for options and the
bundled [configuration examples](xplanet/config/) for choosing maps, labels and
overlays. The bundled ISS orbital elements are historical examples of the TLE
format. Use current orbital elements for live tracking.

If a write fails, Xplanet leaves the existing image intact. Use a writable
directory and a regular output file name. The output path itself cannot
be a symlink, directory or pipe. To use a symlink, specify the real
destination as Xplanet's output and let other programs read it through the link.
Large input textures are supported, including 21,600 × 10,800 global maps.
The renderer rejects oversized decoded images before allocating their main
buffers. Convert CMYK JPEGs to RGB before using them as maps. If an explicitly configured day map cannot be read, a
finite render exits with an error. A running refresh keeps the last image
and tries again next frame. Use `-transpng` for transparent PNG output.
Unknown options return an error; `-help` lists the available options.
The old, unimplemented `-XYZ_file` option is not supported; use
`-origin_file` for supported origin records. Corrected vector normalization
can slightly change pixels in some planetary views. Corrected interpolation
rounding also changes pixel values when relief, cloud or specular maps are
resized to match the day map. These changes are intentional.

## Check a scene before rendering

Put `-validate` first, followed by your render's input options, to check
the scene without rendering a frame, setting wallpaper or running hooks.

```sh
xplanet -validate -target earth -searchdir ./my-scene -config earth.conf
```

It writes one JSON report to stdout and exits with status 0 for a valid
scene or 1 for errors. The report has `schema_version`, `valid` and
`diagnostics` keys. Each diagnostic has a `severity`, `code` and `message`,
with file, line and field details where known. Recoverable issues, such as
a duplicate OMM row or missing cloud image, are warnings and do not fail
validation. An info diagnostic names the selected orbit file. The check parses scene options, config, observer data and
satellite elements, verifies satellite IDs, decodes configured maps and
checks the native JPL header. Other resources, including marker and arc
files, fonts, star catalogs and SPICE inputs, are checked for readability,
not full content or runtime validity. A clean report does not guarantee
render or Finder success.

## Track a satellite with OMM data

Existing TLE and Alpha-5 configurations still work. To use CelesTrak's
OMM CSV, point `satellite_file` to a marker list (for example, `tracked`).
Save the CSV beside it as `tracked.omm.csv` and put the decimal catalog IDs
in `tracked`. Xplanet looks for orbit files in the current directory,
then along the normal search path. The marker list’s location does not
change that order. If both formats are present in the same directory,
it uses TLE.

Use an Earth, TEME, UTC, SGP4 element set. The reader accepts IDs up to nine
digits and reads the CSV directly without converting it to TLE. See the
[satellite input examples](xplanet/satellites/README) and
[CelesTrak's format guide](https://celestrak.org/NORAD/documentation/gp-data-formats.php).

## Try another scene

Catalog stars are visible in the sky around a globe, not on a map that
fills the frame. Omit the projection option to render a globe:

```sh
xplanet -target earth -date 20260320.120000 -geometry 1200x720 \
  -range 8 -radius 38 -starmap BSC -starfreq 0 -num_times 1 -output earth-with-stars.png
```

`BSC` is based on the [Bright Star Catalogue, 5th Revised Edition](https://cdsarc.cds.unistra.fr/viz-bin/cat/V/50).
The globe already uses `BSC` by default; `-starmap` can select a different
catalog. `-starfreq 0` disables random dots where supported, not catalog stars. Add
`-label -font FreeMonoBold.ttf` with a FreeType-enabled build to use the
bundled label font. Local assets can be supplied with `-searchdir`; there
is no need to copy the distributed catalog or font into your configuration.

For more options, see [Earth's atmosphere](xplanet/scattering/README),
[historical flybys](xplanet/origin/README), [a Voyager viewpoint](xplanet/spice/README)
or [higher-precision planetary positions](xplanet/ephemeris/README).
Use [current orbital elements](xplanet/satellites/README) for live
Earth satellite overlays.

## What’s new in 1.5.0 (unreleased)

- Track satellites with OMM CSV and catalog IDs up to nine digits. Existing
  TLE and Alpha-5 setups still work.
- Check a scene before rendering with `-validate` and get a JSON report.

## What’s new in 1.4.0

- Alpha-5 catalog IDs work in satellite element files.
- Large maps are supported, and failed map reads or output writes preserve
  the last good frame.
- On macOS, the Aqua backend waits for Finder’s reply and explains what to
  do if Automation access is denied.
- Existing setups should review the output-path and missing-map behavior
  above. X11 is no longer supported in this fork.

## Platform support

- **macOS (Apple silicon):** Render to a file or use the Aqua backend for
  wallpaper. If macOS asks whether the app launching Xplanet can control
  Finder, allow it if you want Xplanet to set your wallpaper. File output
  does not require that permission.
- **Linux:** The supported mode is headless rendering to a file. Build with
  `--without-x` and render with `-output`. Use your desktop's own settings
  if you want to display the result as wallpaper.
- **X11:** The legacy backend is end-of-life in this fork as of 1.4.0.
  Its code and build options remain, but the backend is no longer tested,
  maintained or supported.

## Build and test

The source archive ships with `configure` and the generated Makefiles.
A C/C++ compiler and Make are needed; Autoconf and Automake are not needed
to build the archive. When building from a Git checkout, run
`./prepare-build.sh` from the source root before configuring. It adjusts
generated-file timestamps without changing their contents.

For maintainers changing `configure.ac`, M4 macros or `Makefile.am`, use
GNU Autoconf 2.71 and Automake 1.16.5 in a clean checkout. Run
`LC_ALL=C autoreconf -fv` and review the generated changes alongside their
inputs. Omit `--install` so Autoreconf keeps the checked-in auxiliary files,
including `config.guess` and `config.sub`. Update those separately.
Regenerate again to check that the output is stable. Build and test
the resulting `make dist` archive without the generator tools on the build
path. Do not hand-edit generated files. Replacing Autotools is a separate
decision from building the current release.

For a headless build on Debian, install `gcc`, `g++`, `make`, `pkgconf`,
`libjpeg62-turbo-dev`, `libpng-dev` and `libfreetype-dev`. From the source
directory, use:

```sh
./configure --prefix="$HOME/.local" --without-x --without-xscreensaver \
  --without-aqua --without-pango --without-cspice --without-pnm \
  --without-gif --without-tiff --with-jpeg --with-png
make
make check
make install
```

The binary is installed at `$HOME/.local/bin/xplanet`. For a macOS source
build with wallpaper support, configure with `--with-aqua --without-x`
and make the codec and font libraries available to the build.

See [the test notes](tests/README.md) for what `make check` covers. The
original [README](README) is preserved as a historical option reference.

## Where this code came from

Hari Nair's original [Xplanet homepage](https://xplanet.sourceforge.net/)
points to [1.3.1, the official release from March 2016](https://xplanet.sourceforge.net/download.php)
and the [upstream Subversion source on SourceForge](https://sourceforge.net/p/xplanet/code/HEAD/tree/trunk/).
To inspect the exact upstream revision this work builds on:

```sh
svn checkout -r 225 svn://svn.code.sf.net/p/xplanet/code/trunk xplanet-r225
```

The Homebrew tap packages a maintained archive based on that Subversion
revision. In [Hari's GitHub repository](https://github.com/mcfiddish/xplanet),
he [imported r225](https://github.com/mcfiddish/xplanet/commit/911c001df1c808abffb884646f6873b78da0c56e)
and updated the build system. This fork starts from that Git history
and carries forward fixes from the tap. The GitHub import is part of the
code's history; the official homepage still points to SourceForge for
upstream source and releases.

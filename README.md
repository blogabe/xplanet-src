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
build, not the `1.3.2` work in this repository.

## Render an image

With Xplanet installed, run:

```sh
xplanet -target earth -projection rectangular -geometry 1200x600 \
  -num_times 1 -output earth.png
```

This writes `earth.png` in the current directory using the bundled Earth map.
Add `-date 20261125.120000` to render a fixed time (November 25, 2026 at noon UTC),
or switch to `-projection orthographic` to see Earth as a globe. For bodies with
no bundled texture, supply your own map. See `man xplanet` for options and the
bundled [configuration examples](xplanet/config/) for choosing maps, labels and
overlays.

## Platform support in 1.3.2

- **macOS (Apple silicon):** Render to a file or use the Aqua backend for
  wallpaper. If macOS asks whether the app launching Xplanet can control
  Finder, allow it if you want Xplanet to set your wallpaper. File output
  does not require that permission.
- **Linux:** The supported mode is headless rendering to a file. Build with
  `--without-x` and render with `-output`. Use your desktop's own settings
  if you want to display the result as wallpaper.
- **X11:** The legacy backend is end-of-life in this fork as of 1.3.2.
  Its code and build options remain, but the backend is no longer tested,
  maintained or supported.

## Build and test

The source archive ships with `configure` and the generated Makefiles.
A C/C++ compiler and Make are needed; Autoconf and Automake are not needed
to build the archive. When building from a Git checkout, run
`./prepare-build.sh` from the source root before configuring. It adjusts
generated-file timestamps without changing their contents.

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

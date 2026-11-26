# Renderer regression checks

Run `make check` from the build directory. The native checks cover:

- Five-digit and Alpha-5 satellite catalog IDs, including name handling.
  OMM CSV checks exercise fractional UTC epochs, six- and nine-digit IDs
  and the propagation path without network access.
- Map downsampling for square, rectangular, odd-sized and one-row maps.
- Image reads and enabled codec round trips, including decoded-size bounds,
  PNG alpha and bit depth, grayscale JPEG, recoverable JPEG warnings, and
  rejection of truncated JPEGs and GIFs without a frame. Failed reads must
  leave the previous image intact.
- Assignment separators are found only within the current line.
- Legacy Rayleigh table loading, vector and orbit math, and trail arguments.

With PNG and JPEG enabled, the CLI checks also render in an isolated temporary
directory. They don't use the current user's maps or change the desktop.

- A uniform texture checks all decoded RGB samples. An asymmetric texture
  checks orientation and longitude on rectangular maps and orientation in
  Lambert and default globe renders. Fixed UTC dates exercise lighting
  near dawn and the seasonal shift at high latitude. Pixel tolerances
  allow for the renderer's texture filtering. A globe also checks that the
  bundled star catalog draws stars in the sky with random stars disabled.
  If FreeType is enabled, the bundled font is exercised on a label.
- File-output checks cover replacement, preservation on write failure,
  temporary-file cleanup, rejected destinations and frame numbering.
- Additional cases check refresh recovery, timezone restoration, observer
  records, invalid options, exact decimal notation for integer settings,
  and continued rendering after overlong annotation and TLE records.

Separate CLI checks compare fixed-date TLE and OMM images and trail
coordinates and exercise six- and nine-digit OMM IDs, companion search
precedence, legacy marker options, bounded propagation warnings, and the
side-effect-free `-validate` JSON and exit status.

## macOS wallpaper release checks

The automated checks cover the fork guard without touching the desktop:
AppKit wallpaper rejects `-fork`, but an AppKit-configured `-fork -output` run
still produces identical pixels. The legacy `-wa` spelling also renders.

Use a disposable GUI session for wallpaper tests. Deny Finder Automation
and verify that a one-frame Finder run exits nonzero, keeps its diagnostic
image and skips the post-render hook. An unattended run should log a failed
publish, retry on schedule, and recover after the cause is cleared. Also
verify the AppKit failure path with a controlled failed publish.

Before rollout, change the active display topology during a running AppKit
session and confirm subsequent refreshes without restarting. Check direct
SSH runs with and without an active desktop login separately from the
supported GUI LaunchAgent. Outside the GUI session, look for a clean
failure rather than a crash, hang, or change to another session's wallpaper.

When building from a Git checkout, run `./prepare-build.sh` before configuring
if timestamps would trigger regeneration. For changes to the build inputs,
use the pinned regeneration procedure in [the project README](../README.md).

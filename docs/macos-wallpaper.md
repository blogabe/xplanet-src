# Xplanet on macOS: wallpaper backend decision

**In the current 1.6 draft**, AppKit sets the real macOS wallpaper by default, with Finder available explicitly. Both can accumulate wallpaper history and cache files.

**Chosen direction:** For frequent updates, draw in an AppKit desktop layer while the user is signed in. This avoids creating wallpaper history on every refresh. The trade-off is that macOS retains its own wallpaper for the lock and login screens and reveals it when Xplanet stops. The desktop layer is a prototype, not yet a selectable backend or the shipping default.

## The problem

Finder and the AppKit wallpaper setter both make the render macOS's wallpaper. Xplanet can delete its own temporary PNG, but that does not remove Apple's separate records: the **Your Photos** gallery entry, the image extension's cached **PNG copy**, and the wallpaper agent's **BMP**.

The requirement is to keep storage and gallery entries bounded during ordinary use, not depend on somebody running a Terminal command later. Existing buildup is a separate recovery problem.

## Options

| Path | Benefits | Limits |
| --- | --- | --- |
| **Finder wallpaper setter** | Real system wallpaper, including the macOS features that use it. Existing compatibility path. | Apple Events approval can block unattended use. Still creates wallpaper artifacts; changing back to Finder does not solve storage. |
| **AppKit wallpaper setter** | Real system wallpaper without the Finder Apple Event. This is the path in renderer PR #3. | Still creates system records and cached images. An API success may precede a visible update. Hidden Spaces and display changes need their own testing. |
| **Two reused paths with a wallpaper setter** | Can limit distinct source identities if that OS and backend reuse them. | Reapplying an overwritten path has visibly left old pixels in the guest. It does not reliably bound BMP creation. Limited source filenames on their own are not the fix. |
| **AppKit desktop layer** | Draws the latest frame in a borderless, mouse-transparent desktop-level window without calling either wallpaper setter. Reuses the same window. None of the three Apple stores grew in the real-render guest test. | The OS wallpaper underneath stays unchanged and returns when the layer exits. This cannot be sold as lock-screen wallpaper or assumed to behave like system wallpaper in Mission Control, fullscreen apps, and all display arrangements. |
| **Explicit image output** | Keeps headless use and external tools independent of the built-in desktop options. | Xplanet owns the output file, not whatever a downstream tool does to system wallpaper stores. If the external tool sets system wallpaper, it must address the same question. |
| **Automatically prune after each refresh or on a timer** | Could retain real system wallpaper and recover disk space without a person intervening. | Has to identify what is still in use across Spaces and displays, avoid racing macOS, and reach protected Apple stores from a background process. Permission was denied for a background probe; even with access the private layout is unsupported. Not the default solution. |
| **Publish less often or pause at a disk budget** | Reduces growth or provides an emergency stop. | Slows or stops the live image and still creates new records. A guard, not prevention. |
| **Manual cleanup** | Separately clears old disk usage. The surgical draft limits deletion to attributable, old Xplanet cache files and protects active and unrelated entries; a full gallery reset is broader. | Apple's stores are private implementation details. Permissions and the risk of breaking unrelated or inactive wallpapers make unattended pruning a poor default. Neither manual cleanup nor a disk warning prevents growth. |

The AppKit name describes two different uses of the framework here. Calling the wallpaper setter asks macOS to register a file. Presenting a desktop-level window asks the window system to show pixels, not to register wallpaper. The prototype temporarily renders to a private PNG, reads it into memory, removes it, then updates the window. Its steady-state absence of files on disk does not mean the handoff is crash-safe.

## What the prototype proved

A logged-in macOS guest displayed new Xplanet frames without adding gallery entries or changing the wallpaper caches. The live image remained visible in Mission Control on the active Space. The underlying wallpaper returned when the test ended. This does not establish behavior on other Spaces, at lock or login, or with multiple displays.

The prototype recovered from failed and corrupt renders, but a slow render stalled refreshes. If the renderer exited successfully without writing a file, a pre-existing staging image could be displayed. A hard kill during handoff could also leave a temporary file. These are reasons to keep the prototype out of the current default.

### Validation details

- **System wallpaper baseline:** 21 sampled PNG/BMP pairs on a working Mac averaged about 25 MiB per refresh. At five-minute intervals with no eviction, that projects to about 7 GiB per day; it is not an eviction measurement.
- **Normal refresh:** An Aqua LaunchAgent displayed three distinct 1920×960 renders five minutes apart on an Apple-silicon macOS 27 guest. Before and after, the gallery had 147 entries, the image cache had 145 copies, and the bitmap cache had 44 files. The gallery hash, cache inventories, and allocated sizes matched; staging was empty after each successful handoff.
- **Rapid refresh:** A single-display 3840×1620 test logged 40 publications at a two-second cadence with no errors or change in the gallery, caches, or staging. Display-process RSS leveled off at about 115 MiB in this roughly 80-second run, excluding the renderer child. This is not a long soak.

## What decides the rollout

1. The desktop layer is the leading candidate for frequent Xplanet updates if it can preserve the desktop interactions people use. It must handle only a fresh, completely decoded frame, retain the last good one on a render failure, and keep the UI responsive when a child stalls. Use a bounded worker with a deadline, and a handoff that cannot publish stale data or build up files after a crash. A fileless handoff is preferable if it stays simple and tested.
2. Verify the supported user LaunchAgent through login, stop/restart, sleep/wake, lock/unlock, and a render-process crash. Check visible pixels on Space switches, Mission Control, fullscreen transitions, desktop icons and click-to-reveal. Test two displays with different scaling plus hot-plug/reconfiguration before promising per-display behavior. Distinguish what the logged-in desktop shows from what macOS shows at lock or login.
3. Independently verify no new gallery, PNG or BMP growth over many real refreshes, bounded renderer/desktop-layer memory, and sensible behavior in headless or SSH sessions. Run with representative dimensions and actual release configuration; a small fixture or VM alone is not enough for every claim.
4. Keep the manual cleanup as separate recovery for existing installations. Do not use routine resets, private preference rewrites, or silent fallback to a setter to make the desktop-layer test pass. If desktop behavior is unacceptable, keep file output and explicit wallpaper-setting choices, and leave the proactive storage problem open rather than imply it is solved.

See the `-wallpaper_backend` section of the [Xplanet manual](../xplanet.1) for the currently implemented choices. The desktop layer has no command-line switch in this draft.

# LiveArea Search

LiveArea Search is a resident SceShell user plugin that adds a native ScePaf search control to the bottom-right corner of the Vita Home LiveArea. It opens the system IME through a PAF `TextBox`, searches the installed application database, and renders matching applications in a virtualized PAF `ListView`. Selecting a row asks SceShell to open that title through its normal `psgm:open` route.

## Runtime behavior

- The Search control is a native lower-right PAF `CornerButton` using Sony's base, highlight, glow, sound, and layout with only the stock ellipsis replaced by a magnifying-glass texture. It accepts front-touch input but is excluded from focus, keycode, and analog routing, so it does not interfere with Home navigation.
- Search input uses the Vita IME through a PAF `TextBox`. Results update progressively as the text changes; the keyboard enter/search action only ends editing and expands the result surface.
- The result page owns the complete `960 × 512` area below the Vita information bar. The bar remains visible, while untextured opaque full-width planes cover the Home wallpaper and bubbles without the rounded transparent edges of Sony's dialog-fill texture. A separate non-interactive `80 × 64` PAF occluder at the extreme top-right covers the higher-priority Home notification bubble without mutating SceShell widgets or obscuring the clock, network state, or battery; it exists only for the result-page lifetime.
- The hardware-validated layout uses one `960 × 64` header and one `960 × 448` ListView for both editing and expanded modes. The complete plugin view loads at priority `36`: behind the system IME view at `35`, but in front of Home at `50`. This ordering applies to the ListView camera as well as ordinary page widgets, so opaque keyboard rows and held-key preview bubbles render above result rows, icons, glows, recycled cells, and overscroll. The result page opens without a page fade and is fully opaque before `BeginEdit`, eliminating the transient half-height layout. Editing changes only focus/control state; it does not resize or reposition the list, replace its camera box, or rebuild its model. Dismissing and reopening the IME is therefore seamless.
- Back uses PAF's semantic `PAD_ESCAPE` action rather than a physical Circle/Cross mask. While the TextBox is editing, Back ends IME editing and keeps Search open; a later Back, after editing is inactive, invokes the verified deferred page close. This preserves the system's X/O assignment.
- One PS-button press closes Search whether the IME is active or already dismissed. Search acquires SceShell's PS-button lock before becoming interactive, observes the unmodified PAF pad sample ahead of listener dispatch, quiesces the IME and result page, and releases the shell lock only after the physical button is up, preventing an unintended Home action from the same press.
- Results use PAF's vertical list layout, kinetic scrolling, focus handling, and 70 px common-list geometry.
- Each row uses a native `app_icon_simple` widget and one title line; title-ID metadata is deliberately omitted.
- Artwork is loaded asynchronously from LiveArea's plaintext `ur0:appmeta/<TITLE_ID>/icon0.png` copy. Visible rows pin their surfaces. Under surface-pool pressure, the cache releases only an unpinned, cache-owned least-recently-used surface and retries the load; evicted entries retain their source path and reload when revisited. Missing source files remain neutral placeholders without evicting valid artwork.
- Results are ranked deterministically: exact title/title-ID, prefix, token prefix, substring, then metadata matches.
- An empty query shows `Recent`: up to 20 unique applications most recently selected through LiveArea Search. Selecting an older entry moves it to the front, unavailable apps are omitted, and the bounded MRU order persists across reboot in `ur0:data/livearea-search/recent_apps.txt`. Saves use a verified same-directory temporary file plus a recoverable backup transaction because Vita file renames do not replace an existing destination.
- Recency is recorded and saved before SceShell is asked to open the title, so a failed save is logged without blocking the open attempt and a failed title-open request does not lose the in-memory selection.
- Launching uses `sceAppMgrLaunchAppByUri(0x20000, "psgm:open?titleid=...")`, so SceShell retains normal application lifecycle ownership.
- The overlay fails closed and hides whenever an application owns the foreground or the required shell state cannot be read. The Home control additionally checks the verified topmenu Home page and horizontal-scroll target, so it disappears as soon as a transition leaves Home and begins its short native fade as soon as a returning bubble page is visibly entering.
- SceLsdb indexing and Recent-history I/O run on the worker thread before the PAF handoff. The worker treats SceShell's unopened AppDB result as a readiness state and retries without blocking the render thread. Before loading its own RCO, the plugin also verifies that SceShell's standalone loading Scene in PAF environment entry `25` has been destroyed; that Scene owns the full-screen black plane and center spinner used during cold boot. This is a firmware-backed lifecycle gate rather than a frame delay.
- Closing during kinetic scrolling uses a one-frame deferred teardown: input and focus are quiesced before rows, icon notifications, pins, page ownership, and model storage are released in verified order.
- Before any PAF or worker initialization, startup requires one exact active `ur0:tai/livearea_search.suprx` or `ur0:/tai/livearea_search.suprx` line in `ur0:tai/config.txt`, replaces only that line's first byte with `#`, synchronizes the file, and verifies the complete file byte-for-byte. The current boot continues with the already-loaded module, while a reboot leaves the plugin disabled.
- Startup returns `SCE_KERNEL_START_NO_RESIDENT` if the entry is missing or duplicated, or if the configuration cannot be read, changed, synchronized, and verified, rather than continuing without the reboot fail-safe.

The implementation does not patch SceShell instructions or use firmware-specific offsets. It does resolve SceLsdb exports by NID at runtime, so hardware validation across the intended firmware and hardware variants remains required.

## Host tests

```sh
cmake -S . -B build-host \
    -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
    -DLIVEAREA_SEARCH_BUILD_VITA=OFF
cmake --build build-host --parallel 4
ctest --test-dir build-host --output-on-failure
```

The host suite covers query normalization, ranking, stable ordering, result limits, catalog-record policy, title-ID validation, LiveArea URI construction, startup self-disable matching, worker-prepared catalogue ownership transfer, AppDB readiness classification, the firmware-backed shell-loading Scene gate, exact Home-surface recognition, PS-button ownership and lock release, IME transition/adoption policy, two-stage semantic Back handling, deterministic pin-aware icon eviction, Recent MRU ordering/filtering/serialization, recoverable persistence ordering, notification-occluder lifecycle, deferred close ordering, plugin-view ordering behind the IME, full-list editing composition, fade-free result-page opening, the prohibition on per-frame camera resets, and full-screen PAF resource contracts.

## Vita build

The target requires:

- Docker;
- a VitaSDK image containing the ARM toolchain and `vita_create_self` tools;
- a complete ScePaf-RE component root containing `include/paf.h` and `lib/libScePaf_stub_weak.a`;
- an executable `psp2cxml-tool` host binary.

`tools/build-vita.sh` defaults to the dependency copies in the sibling VitaXMB checkout used by this workspace and to the pinned VitaSDK image digest `sha256:99814094e0240b7ebe8700ba3742ee761bbcfb22bf8eedeac8db5c1dbe3e3d32`. The verified dependency revisions are:

- `vitasdk-paf-component`: `9a68b07aa2d7247a756765bf154f292cda20aed3`;
- `psp2cxml-tool`: `d971a9a9892c122b4edef15d335326b2a551fec4`.

Override any location as needed:

```sh
LIVEAREA_SEARCH_PAF_ROOT=/absolute/path/to/paf-component/final \
PSP2CXML=/absolute/path/to/psp2cxml-tool \
VITASDK_IMAGE=your-vitasdk-image \
tools/build-vita.sh
```

Optional variables:

- `LIVEAREA_SEARCH_BUILD_DIR` — repository-relative output directory, default `build-vita`;
- `LIVEAREA_SEARCH_JOBS` — parallel build count, default `4`.

Successful output is staged as:

```text
build-vita/dist/livearea_search.suprx
build-vita/dist/ur0/data/livearea-search/livearea_search_plugin.rco
```

## Installation

1. Back up `ur0:tai/config.txt`.
2. Copy `livearea_search.suprx` to `ur0:tai/livearea_search.suprx`.
3. Copy `livearea_search_plugin.rco` to `ur0:data/livearea-search/livearea_search_plugin.rco`.
4. Add the plugin to the `*main` section of `ur0:tai/config.txt` as shown in `deploy/config.txt.example`.
5. Restart the Vita.

On that startup, the plugin immediately changes the active entry to `#r0:tai/livearea_search.suprx` (or `#r0:/tai/livearea_search.suprx` for the slash form). This is intentional: if the resident plugin later destabilizes SceShell, a normal reboot will not load it again. To run the plugin on a later boot, restore the initial `u` in the configuration entry before rebooting.

If a failure occurs before the module reaches `module_start`, hold **L** during boot to bypass taiHEN plugins, then repair the configuration.

## Current verification status

The portable host tests pass under normal and ASan/UBSan builds, and the complete ARM target builds without warnings under the documented GCC 15 VitaSDK container, including SUPRX conversion and RCO generation. On retail Vita 146, native `960 × 544` captures verified that the magnifier remains absent throughout the cold-boot black-spinner Scene and fades only after rendered Home bubbles are visible; it also begins fading in the first visible frame of a title-LiveArea return. The startup fail-safe, nonblocking AppDB preparation, Home-only gate, native touch-only magnifier control, immediate opaque full-list opening, isolated notification-bubble occlusion and restoration, persistent Recent mode, progressive search, keyboard re-entry, held-key IME preview layering, keyboard-over-list composition, touch and kinetic scrolling while the IME is open, expanded-list scrolling, two-stage Back behavior, one-press PS closure with and without the IME, 128-row controller scrolling, title-only rows, surface-pool eviction/reload in both scroll directions, missing-icon fallback, title opening, and deferred kinetic-flick close were exercised successfully. Static firmware analysis confirmed the same shell loading-Scene construction on the internal/debug 4.00 lineage; runtime variants beyond Vita 146 remain unverified.

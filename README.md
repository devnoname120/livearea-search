# LiveArea Search

LiveArea Search is a resident SceShell user plugin that adds a native ScePaf search control to the bottom-right corner of the Vita Home LiveArea. It opens the system IME through a PAF `TextBox`, searches the installed application database, and renders matching applications in a virtualized PAF `ListView`. Selecting a row asks SceShell to open that title through its normal `psgm:open` route.

## Runtime behavior

- The Search control is a native lower-right PAF `CornerButton` using Sony's base, highlight, glow, sound, and layout with only the stock ellipsis replaced by a magnifying-glass texture. It accepts front-touch input but is excluded from focus, keycode, and analog routing, so it does not interfere with Home navigation.
- Search input uses the Vita IME through a PAF `TextBox`. Results update progressively as the text changes; the keyboard enter/search action only ends editing and expands the result surface.
- While the IME is active, the plugin uses a compact dark result region that ends immediately above the keyboard. Tapping the search field after submission reopens the IME and returns to this compact layout.
- Results use PAF's vertical list layout, kinetic scrolling, focus handling, and 70 px common-list geometry.
- Each row uses a native `app_icon_simple` widget and one title line; title-ID metadata is deliberately omitted.
- Artwork is loaded asynchronously from LiveArea's plaintext `ur0:appmeta/<TITLE_ID>/icon0.png` copy. Visible rows pin their surfaces. Under surface-pool pressure, the cache releases only an unpinned, cache-owned least-recently-used surface and retries the load; evicted entries retain their source path and reload when revisited. Missing source files remain neutral placeholders without evicting valid artwork.
- Results are ranked deterministically: exact title/title-ID, prefix, token prefix, substring, then metadata matches.
- Launching uses `sceAppMgrLaunchAppByUri(0x20000, "psgm:open?titleid=...")`, so SceShell retains normal application lifecycle ownership.
- The overlay fails closed and hides whenever an application owns the foreground or the required shell state cannot be read. The Home control additionally checks the verified topmenu Home page and horizontal-scroll focus/target, so it is absent from title LiveAreas, PlayStation Store, and other PAF surfaces.
- The plugin waits until SceShell permits overlays and a visible topmenu page remains stable for 60 consecutive PAF frames before loading its own RCO.
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

The host suite covers query normalization, ranking, stable ordering, result limits, catalog-record policy, title-ID validation, LiveArea URI construction, startup self-disable matching, exact Home-surface recognition, IME transition/adoption policy, deterministic pin-aware icon eviction, and PAF resource composition contracts.

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

The portable host tests pass, and the complete ARM target builds without warnings under the documented GCC 15 VitaSDK container, including SUPRX conversion and RCO generation. On Vita 146, the startup fail-safe, verified Home-only gate, native touch-only magnifier control, unobscured IME, progressive `Z` and `A` queries, keyboard re-entry, 128-row controller scrolling, full-height and compact result geometry, title-only rows, surface-pool eviction/reload in both scroll directions, missing-icon fallback, and `MAXP00000` LiveArea launch were exercised successfully. Screenshots also confirm that the control is absent from PlayStation Store and title LiveAreas. Firmware and hardware variants beyond that device still require validation.

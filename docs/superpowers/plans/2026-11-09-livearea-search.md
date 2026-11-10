# LiveArea Search Implementation Plan

> Execute on branch `dev/livearea-search`. Keep the portable search core buildable on the host; isolate all ScePaf, taiHEN, and SceLsdb dependencies behind Vita-only translation units.

## Task 1: Establish repository and host test harness

**Files:** `.gitignore`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_app_index.cpp`, `include/livearea_search/app_index.h`, `include/livearea_search/launch_uri.h`

1. Add `.superpowers/`, build directories, generated RCO outputs, and target artifacts to `.gitignore`.
2. Add a CMake host mode with CTest and a dependency-free test executable.
3. Write failing tests for normalization, exact/prefix/token/substring ranking, stable tie ordering, result limits, and launch URI validation.
4. Run configure/build/CTest and confirm the tests fail because the implementation is absent.

## Task 2: Implement portable index and URI builder

**Files:** `src/core/app_index.cpp`, `src/core/launch_uri.cpp`, headers and tests from Task 1

1. Implement ASCII/UTF-8-safe normalization without locale-dependent global state.
2. Precompute normalized searchable fields when applications are loaded.
3. Implement deterministic score tuples and result limits.
4. Implement strict title-ID validation and bounded `psgm:open?titleid=` construction.
5. Run host tests until all pass; add edge cases discovered during implementation.

## Task 3: Define minimal firmware-verified SceLsdb ABI

**Files:** `include/livearea_search/lsdb_abi.h`, `src/vita/lsdb_index_source.{h,cpp}`

1. Define only the verified structures, callback types, library NIDs, and function NIDs needed by this plugin.
2. Resolve exports through taiHEN at runtime.
3. Initialize/end the lightweight AppDatabase token around enumeration.
4. Copy title ID, localized title, application name, and category synchronously in the borrowed callback.
5. Exclude non-launchable/malformed rows and feed copied records to `AppIndex`.
6. Add host-side tests for the callback-copy/filter policy through a small abstraction or fixture.

## Task 4: Add ScePaf resources and generated identifiers

**Files:** `resources/livearea_search_plugin.xml`, `resources/livearea_search_plugin.h`, `tools/fnv1a.py`, `resources/CMakeLists.txt`

1. Define the native lower-right touch-only Search `CornerButton`, modal result page, header, ListView, empty-state label, lower-left close control, and title-only row template.
2. Reuse the common corner base/highlight/glow/sound/layout and replace only its icon object with a bundled magnifier texture.
3. Use a full 960 x 544 result surface, a compact header-to-keyboard editing surface, fixed 70 px common-list cells, and 64 px art.
4. Generate and verify stable FNV-1a identifiers from resource names.
5. Add build commands for RCO generation when the PAF resource tools are available; produce a clear configuration error otherwise.

## Task 5: Implement the PAF list and IME flow

**Files:** `src/vita/search_overlay.{h,cpp}`, `src/vita/result_item_factory.{h,cpp}`, `src/vita/icon_loader.{h,cpp}`

1. Load the plugin-owned RCO and open the Home control page on PAF's main thread.
2. Bind search/close/list-row handlers with explicit controller lifetime.
3. Configure a PAF TextBox, start the native IME on search activation, and update results whenever its normalized text changes.
4. Use the compact result region while editing; on Enter, end editing, wait for shell visibility recovery, and restore the full result surface without rebuilding an unchanged query.
5. Detect native TextBox editing started by tapping the submitted field and adopt it into the IME transition state.
6. Generate/recycle PAF ListItem rows and populate one title line without title-ID metadata.
7. Resolve icon paths from AppDB and decode into PAF surfaces asynchronously. Pin visible rows, evict only unpinned cache-owned LRU surfaces under pool pressure, retry the load, and preserve source records for reverse-scroll reload.
8. Keep all widget mutation on the PAF main thread and make close/teardown idempotent.

## Task 6: Implement shell startup, visibility, and launch

**Files:** `src/vita/plugin_main.cpp`, `src/vita/shell_lifecycle.{h,cpp}`, `src/vita/launch.cpp`

1. Wait for the stable ScePaf module boundary without patching firmware offsets.
2. Load the overlay controller through PAF's main-thread call list after AppMgr state is available.
3. Verify the topmenu Home page and horizontal ScrollView focus/target, and show the control only on the Home surface rather than title LiveAreas, Store, or other PAF pages.
4. Build a validated `psgm:open?titleid=` URI and dispatch it with `sceAppMgrLaunchAppByUri(0x20000, uri)`.
5. Cleanly stop before UI creation and reject unsafe unload after the resident PAF page is active.

## Task 7: Add Vita build/install metadata and documentation

**Files:** `cmake/VitaShellPlugin.cmake`, `README.md`, `deploy/config.txt.example`, `tools/build-vita.sh`

1. Define Vita compiler flags, ScePaf/taiHEN/AppMgr dependencies, SUPRX packaging, RCO generation, and install staging.
2. Document exact dependency versions/paths and deployment steps.
3. Document firmware assumptions, SceLsdb ownership constraints, and why URI launch is used.
4. Document host tests and target/on-device verification steps.

## Task 8: Verify and review

1. Run host configure/build/CTest from a clean build directory, including Home-surface, IME-adoption, icon-eviction, and resource-composition contracts.
2. Run formatting/static checks and scan for unbounded string operations, retained borrowed pointers, and non-main-thread widget mutation.
3. Build the complete ARM target with the pinned VitaSDK image, inspect imports/sections/runtime symbols, and stage the SUPRX/RCO install tree.
4. Deploy with readback-verified backups and the one-shot config fail-safe; validate Home/Store/title visibility, touch-only input, progressive search, compact/full geometry, keyboard re-entry, forward/reverse cache pressure, and launch on Vita 146.
5. Inspect the full project diff once.
6. Request a code-review pass focused on firmware ABI, lifecycle races, PAF ownership, list virtualization, and surface lifetime.
7. Address findings, rerun verification, and commit coherent changes.

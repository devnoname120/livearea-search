# Results-Count Right Inset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move `Recent` / `<n> results` 16 pixels inward while preserving the 750-pixel search box and structurally preventing a four-digit count from overlapping it.

**Architecture:** Keep the existing fixed header partition. Change only the result-label layout position from `0,0` to `-16,0`; retain its 150-pixel width, right alignment, and ellipsis behavior. Strengthen CMake contracts so the unchanged search box, the new inset, and the resulting 26-pixel non-overlap gap are explicit and regression-tested.

**Tech Stack:** CXML/RCO layout resources, CMake/CTest source contracts, C++11 Vita plugin, VitaSDK MinSizeRel builds, self-disabling diagnostic deployment, retail Vita 146 native capture.

---

## File map

- Modify `tests/check_search_polish.cmake`: require the new result-label position, preserve the search-box geometry, and assert the fixed regions have a 26-pixel gap.
- Modify `tests/check_notification_corner.cmake`: replace the former natural-edge requirement with the approved 16-pixel inset and forbid both historical positions.
- Modify `resources/livearea_search_plugin.xml`: move only `text_results_count` to `pos="-16,0"`.
- Modify `docs/superpowers/specs/2026-11-15-results-count-inset-design.md`: append final verification hashes and evidence after hardware qualification.
- Create ignored hardware receipts under `build-vita-regression-diagnostic/`; do not commit those files.

### Task 1: Lock the geometry with failing contracts

**Files:**
- Modify: `tests/check_search_polish.cmake:21-31`
- Modify: `tests/check_notification_corner.cmake:62-81`
- Test: `livearea_search_polish_contract`
- Test: `livearea_search_notification_corner`

- [ ] **Step 1: Update the polish contract to require the approved partition**

Replace the result-label marker and add explicit geometry arithmetic:

```cmake
foreach(required_resource IN ITEMS
    "<layout_hint align=\"1,1\" anchor=\"1,1\" size=\"750,58\" pos=\"18,0\" />"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"-16,0\" />"
    "font_size=\"22\""
    "line_pitch=\"32\""
    "align_x=\"2\""
    "ellipsis=\"1\""
    "id=\"style_results_count\"")
```

Then add:

```cmake
set(results_header_width 960)
set(search_box_left 18)
set(search_box_width 750)
set(results_count_width 150)
set(results_count_right_inset 16)
math(EXPR search_box_right "${search_box_left} + ${search_box_width}")
math(EXPR results_count_left
    "${results_header_width} - ${results_count_right_inset} - ${results_count_width}")
math(EXPR results_header_gap "${results_count_left} - ${search_box_right}")
if(NOT results_header_gap EQUAL 26)
    message(FATAL_ERROR
        "Results header regions must retain a 26-pixel non-overlap gap")
endif()
```

- [ ] **Step 2: Update the notification/layout contract**

Require:

```cmake
string(FIND "${resource_xml}"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"-16,0\" />"
    inset_results_count)
if(inset_results_count EQUAL -1)
    message(FATAL_ERROR
        "The Recent/results-count label must use the approved 16-pixel right inset")
endif()
```

Forbid both superseded exact layout records:

```cmake
foreach(forbidden_resource IN ITEMS
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"0,0\" />"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"-80,0\" />")
```

- [ ] **Step 3: Run focused contracts and verify RED**

Run:

```sh
cmake \
  -DRESOURCE_XML="$PWD/resources/livearea_search_plugin.xml" \
  -DSEARCH_OVERLAY="$PWD/src/vita/search_overlay.cpp" \
  -DOVERLAY_HEADER="$PWD/src/vita/search_overlay.h" \
  -DPAF_TEXT_BOX_IME_HEADER="$PWD/src/vita/paf_text_box_ime.h" \
  -DPAF_PAD_HANDLER_HEADER="$PWD/src/vita/paf_pad_device_handler.h" \
  -DVITA_CMAKE="$PWD/cmake/VitaShellPlugin.cmake" \
  -P tests/check_search_polish.cmake
cmake \
  -DRESOURCE_XML="$PWD/resources/livearea_search_plugin.xml" \
  -DNOTIFICATION_HEADER="$PWD/src/vita/notification_corner.h" \
  -DNOTIFICATION_SOURCE="$PWD/src/vita/notification_corner.cpp" \
  -DOVERLAY_HEADER="$PWD/src/vita/search_overlay.h" \
  -DSEARCH_OVERLAY="$PWD/src/vita/search_overlay.cpp" \
  -P tests/check_notification_corner.cmake
```

Expected: both fail because production still contains `pos="0,0"`.

### Task 2: Apply the minimal resource change

**Files:**
- Modify: `resources/livearea_search_plugin.xml:30-32`
- Test: focused CMake contracts

- [ ] **Step 1: Move only the result-label region**

Change:

```xml
<layout_hint align="2,1" anchor="2,1" size="150,64" pos="0,0" />
```

to:

```xml
<layout_hint align="2,1" anchor="2,1" size="150,64" pos="-16,0" />
```

Do not change the search-box `size="750,58" pos="18,0"`, font size, alignment, ellipsis, or result-count formatting.

- [ ] **Step 2: Run focused contracts and verify GREEN**

Repeat the two commands from Task 1.

Expected: both exit 0.

- [ ] **Step 3: Verify the formatter still covers the requested forms**

Run:

```sh
grep -n 'SetText(result_count_, "Recent")\|%u result\|%u results' \
  src/vita/search_overlay.cpp
```

Expected: markers for `Recent`, singular `1 result`, and plural counts are present. The 32-byte buffer can hold `9999 results` with termination.

### Task 3: Run complete offline verification and build exact Vita artifacts

**Files:**
- No tracked source changes beyond Tasks 1-2.
- Create ignored build directories only.

- [ ] **Step 1: Run a fresh normal host graph**

```sh
cmake -S . -B build-host-results-count-inset \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF
cmake --build build-host-results-count-inset --parallel 4
ctest --test-dir build-host-results-count-inset --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 2: Run a fresh ASan/UBSan graph**

```sh
cmake -S . -B build-host-results-count-inset-sanitized \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-host-results-count-inset-sanitized --parallel 4
ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir build-host-results-count-inset-sanitized --output-on-failure
```

Expected: all tests pass with no sanitizer findings.

- [ ] **Step 3: Run resource and source-contract audits**

```sh
python3 tools/firmware/resource_compatibility_audit.py \
  --root /Volumes/data/Downloads/vita \
  --manifest tools/firmware/resource_compatibility_manifest.json \
  --json-out build-results-count-inset-resource-audit.json \
  --markdown-out build-results-count-inset-resource-audit.md
python3 tools/firmware/source_contract_audit.py \
  --root . \
  --contract tools/firmware/source_firmware_contract.json \
  --json-out build-results-count-inset-source-audit.json \
  --markdown-out build-results-count-inset-source-audit.md
```

Expected: resource compatibility remains 84/84 and source contracts remain 26/26.

- [ ] **Step 4: Build warning-free release and self-disabling artifacts**

```sh
LIVEAREA_SEARCH_BUILD_DIR=build-vita-results-count-inset-release \
LIVEAREA_SEARCH_BUILD_TYPE=MinSizeRel \
LIVEAREA_SEARCH_DIAGNOSTICS=OFF \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=OFF \
  sh tools/build_vita.sh
LIVEAREA_SEARCH_BUILD_DIR=build-vita-results-count-inset-experimental \
LIVEAREA_SEARCH_BUILD_TYPE=MinSizeRel \
LIVEAREA_SEARCH_DIAGNOSTICS=ON \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=ON \
  sh tools/build_vita.sh
```

Expected: both builds exit 0, their binary audits pass, and both RCO files are byte-identical.

- [ ] **Step 5: Record exact hashes**

```sh
shasum -a 256 \
  build-vita-results-count-inset-release/livearea_search_vita.velf \
  build-vita-results-count-inset-release/dist/livearea_search/livearea_search.suprx \
  build-vita-results-count-inset-release/dist/livearea_search/livearea_search_plugin.rco \
  build-vita-results-count-inset-release/livearea_search.zip \
  build-vita-results-count-inset-experimental/livearea_search_vita.velf \
  build-vita-results-count-inset-experimental/dist/livearea_search/livearea_search.suprx \
  build-vita-results-count-inset-experimental/dist/livearea_search/livearea_search_plugin.rco
```

Expected: seven hashes are printed and the two RCO hashes match.

### Task 4: Qualify the exact layout on retail Vita 146

**Files:**
- Create ignored `build-vita-regression-diagnostic/deploy_results_count_inset_experimental.py` by copying the current guarded native-notification deployment harness and replacing only build paths, expected hashes, and evidence prefix.
- Create ignored `build-vita-regression-diagnostic/capture_results_count_inset.py` by copying the current release visual harness and replacing expected hashes and evidence prefix.
- Do not modify tracked production files.

- [ ] **Step 1: Guarded-deploy the self-disabling candidate**

```sh
cp build-vita-regression-diagnostic/deploy_native_notification_final_experimental.py \
  build-vita-regression-diagnostic/deploy_results_count_inset_experimental.py
```

Update these constants in the copied script to the exact Task 3 build paths and hashes, and set the evidence prefix to `results-count-inset-experimental`. Run:

```sh
python3 build-vita-regression-diagnostic/deploy_results_count_inset_experimental.py
```

Expected: exact experimental hashes, zero active/one disabled config entry after startup, unchanged core fingerprints, and `overlay active` in the diagnostic log.

- [ ] **Step 2: Capture the native header and verify the four-digit boundary statically**

Use the diagnostic build to open Search, dismiss IME, and capture the results header with both `Recent` and an actual numeric result label. Save native 960 x 544 frames under an ignored evidence directory.

The four-digit safety requirement is structural rather than dependent on the current catalog size: the exact XML contracts prove a 150-pixel clipped/ellipsized count region, the arithmetic contract proves a 26-pixel gap from the unchanged search box, and the formatter contract proves `9999 results` fits the 32-byte output buffer. Do not add a diagnostic production hook solely to synthesize an unavailable catalog count.

Acceptance:

- `Recent` and the observed numeric result label end 16 pixels before the screen edge;
- the search box remains at its former width and position;
- neither label touches or overlaps the search box;
- the static four-digit boundary contracts remain green;
- no notification bubble, black occluder, or layout shift returns;
- Search close restores the native notification bubble;
- exact candidate hashes/config/core fingerprints remain unchanged.

- [ ] **Step 3: Install and verify the matching permanent release**

Copy the current guarded permanent native-notification deployment harness, update only build paths/hashes/evidence prefix, and run it. Require two clean boots, one active/zero disabled config entries, exact release hashes, Search/Back smoke, stable startup-log bytes, and unchanged core fingerprints.

### Task 5: Finalize evidence, commit, and push

**Files:**
- Modify: `docs/superpowers/specs/2026-11-15-results-count-inset-design.md`
- Commit: tests, XML, spec, and plan

- [ ] **Step 1: Append final evidence**

Record in the design spec:

- final release/experimental SUPRX and shared RCO hashes;
- normal and sanitizer test totals;
- resource/source audit totals;
- experimental and permanent evidence paths;
- native acceptance result for `Recent` and `9999 results`;
- stable startup-log and core-set result.

- [ ] **Step 2: Run final verification after documentation changes**

```sh
git diff --check
ctest --test-dir build-host-results-count-inset --output-on-failure
ASAN_OPTIONS=detect_leaks=0 \
  ctest --test-dir build-host-results-count-inset-sanitized --output-on-failure
python3 tools/firmware/source_contract_audit.py \
  --root . \
  --contract tools/firmware/source_firmware_contract.json \
  --json-out build-results-count-inset-source-audit-final.json \
  --markdown-out build-results-count-inset-source-audit-final.md
```

Expected: clean diff, all tests pass, source contracts pass.

- [ ] **Step 3: Review the aggregate diff**

Verify that tracked changes are limited to:

```text
docs/superpowers/specs/2026-11-15-results-count-inset-design.md
docs/superpowers/plans/2026-11-15-results-count-inset.md
resources/livearea_search_plugin.xml
tests/check_search_polish.cmake
tests/check_notification_corner.cmake
```

- [ ] **Step 4: Commit and push**

```sh
git add \
  docs/superpowers/specs/2026-11-15-results-count-inset-design.md \
  docs/superpowers/plans/2026-11-15-results-count-inset.md \
  resources/livearea_search_plugin.xml \
  tests/check_search_polish.cmake \
  tests/check_notification_corner.cmake
git commit -m "Inset results count from screen edge"
git push origin main
```

- [ ] **Step 5: Verify remote synchronization and no publication**

```sh
git fetch origin main
git rev-parse HEAD
git rev-parse origin/main
gh api repos/devnoname120/livearea-search/commits/main --jq .sha
git status --short --branch
git tag -l v1.0.4
git ls-remote --tags origin refs/tags/v1.0.4
gh release view v1.0.4 >/dev/null 2>&1; test $? -ne 0
```

Expected: all three commit IDs match, worktree is clean, and no `v1.0.4` tag or release exists.

# Magnifier Geometry Adjustment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enlarge the Search magnifier lens while shortening and thinning its handle, without changing the established skeuomorphic treatment, runtime layout, or behavior.

**Architecture:** Keep `tools/assets/generate_skeuomorphic_magnifier.py` as the sole source of truth. Lock the requested proportions and deterministic output in `tests/test_home_icon_assets.py`, regenerate the runtime and review PNGs from that generator, then pass the unchanged RCO/build pipeline and all existing audits.

**Tech Stack:** Python 3 deterministic SDF rasterizer, PNG/zlib encoding, CTest, CMake, VitaSDK, `psp2cxml-tool`, existing firmware/resource/binary audit tools.

---

## File map

- Modify `tools/assets/generate_skeuomorphic_magnifier.py`: change only the six lens/handle geometry parameters.
- Modify `tests/test_home_icon_assets.py`: lock the new geometry and deterministic native/rendered PNG metrics.
- Regenerate `resources/texture/search.png`: runtime asset.
- Regenerate `docs/assets/livearea-search-magnifier.png`: byte-identical review copy.
- Modify `docs/firmware/common-corner-back-arrow.md`: record the revised proportions, hash, pixel statistics, and fresh build outputs.
- Preserve `resources/livearea_search_plugin.xml`: the existing `48 × 48` plane, position, tint, glow, highlight, sound, and resource reference must remain unchanged.

### Task 1: Turn the requested proportions into a RED contract

**Files:**
- Modify: `tests/test_home_icon_assets.py:14-16,224-281`
- Test: `tests/test_home_icon_assets.py`

- [ ] **Step 1: Add an exact generator-geometry contract**

After loading the generator in `verify_magnifier`, require these values before inspecting the PNG:

```python
    generator = load_generator(root)
    expected_geometry = {
        "cx": 26.7,
        "cy": 25.8,
        "outer": 15.3,
        "inner": 10.0,
        "hx0": 37.2,
        "hy0": 36.3,
        "hx1": 49.0,
        "hy1": 48.1,
        "handle": 3.8,
    }
    for key, expected in expected_geometry.items():
        if generator.PARAMETERS.get(key) != expected:
            raise AssertionError(
                f"magnifier geometry {key!r}: "
                f"{generator.PARAMETERS.get(key)!r} != {expected!r}"
            )
```

Use `generator.generate_png()` in the existing deterministic reproduction check.

- [ ] **Step 2: Replace old deterministic output expectations**

Set:

```python
MAGNIFIER_SHA256 = "c16942c7a2805306fe84d09621bb4b91f6df7ab027b1ed54a41ad4e7e91b4d95"
```

Require the revised native metrics:

```python
bounding_box(pixels) == (10, 9, 55, 55)
len(pixels) == 1061
sum(color[3] >= 240 for _, _, color in pixels) == 112
```

Keep the existing alpha range `(1, 255)`, grayscale, transparent center, transparent corner, bright-body minimum, and low-alpha edge constraints.

Require the revised rendered `48 × 48` metrics:

```python
bounding_box(rendered_pixels) == (7, 7, 41, 41)
len(rendered_pixels) == 624
len(rendered_edge) == 201
sum(color[3] >= 240 for _, _, color in rendered_pixels) == 52
```

- [ ] **Step 3: Run the focused contract and verify RED**

Run:

```sh
python3 tests/test_home_icon_assets.py --root .
```

Expected: FAIL at the new geometry assertion because the generator still contains `outer == 14.2` and the old handle values.

### Task 2: Implement only the approved geometry change

**Files:**
- Modify: `tools/assets/generate_skeuomorphic_magnifier.py:12-32`
- Modify: `resources/texture/search.png`
- Modify: `docs/assets/livearea-search-magnifier.png`
- Test: `tests/test_home_icon_assets.py`

- [ ] **Step 1: Update the six geometry values**

Change only these entries in `PARAMETERS`:

```python
    "outer": 15.3,
    "inner": 10.0,
    "hx0": 37.2,
    "hy0": 36.3,
    "hx1": 49.0,
    "hy1": 48.1,
    "handle": 3.8,
```

Leave the center, shadow, body alpha, directional alpha, highlight, shade, specular, antialiasing scale, and encoder unchanged.

- [ ] **Step 2: Regenerate both byte-identical assets**

Run:

```sh
python3 tools/assets/generate_skeuomorphic_magnifier.py \
  --output resources/texture/search.png
cp resources/texture/search.png docs/assets/livearea-search-magnifier.png
```

Expected SHA-256 for both files:

```text
c16942c7a2805306fe84d09621bb4b91f6df7ab027b1ed54a41ad4e7e91b4d95
```

- [ ] **Step 3: Run the focused contract and verify GREEN**

Run:

```sh
python3 tests/test_home_icon_assets.py --root .
```

Expected: `home icon asset contracts passed`.

- [ ] **Step 4: Commit the geometry implementation**

```sh
git add \
  tools/assets/generate_skeuomorphic_magnifier.py \
  tests/test_home_icon_assets.py \
  resources/texture/search.png \
  docs/assets/livearea-search-magnifier.png
git commit -m "Refine magnifier proportions"
```

### Task 3: Perform visual QA at source and runtime sizes

**Files:**
- Read: `resources/texture/search.png`
- Read: `docs/assets/sony-common-corner-back-arrow.png`
- Create only under ignored `build-analysis/magnifier-geometry-adjustment/`: visual QA composites

- [ ] **Step 1: Render the current source asset at native and runtime sizes**

Use `decode_png` and `resize_premultiplied_bilinear` from `tests/test_home_icon_assets.py` to produce:

```text
build-analysis/magnifier-geometry-adjustment/native-transparent.png
build-analysis/magnifier-geometry-adjustment/runtime-transparent.png
build-analysis/magnifier-geometry-adjustment/comparison-navy.png
build-analysis/magnifier-geometry-adjustment/comparison-vita-blue.png
```

The comparison canvases must place the exact Sony Back arrow, the previous magnifier from commit `0d2e6ba`, and the revised magnifier side by side without resampling the `64 × 64` source cells. Runtime cells must use the same premultiplied-bilinear `48 × 48` resize used by the contract.

- [ ] **Step 2: Inspect the composites**

Require all of the following:

- the lens is visibly larger than the previous revision;
- the handle is visibly shorter and thinner;
- the transparent center remains clearly open;
- the lower-right handle does not touch the canvas edge;
- the icon remains visually balanced in the unchanged runtime plane;
- highlights and dark edges remain legible on navy and Vita-blue without looking heavier than the stock arrow.

If any condition fails, adjust only the geometry values, regenerate metrics, and repeat Tasks 1–3. Do not change alpha or lighting parameters to compensate for geometry.

### Task 4: Update provenance and build evidence

**Files:**
- Modify: `docs/firmware/common-corner-back-arrow.md:95-170`

- [ ] **Step 1: Update the asset section**

Record:

- revised PNG SHA-256 `c16942c7a2805306fe84d09621bb4b91f6df7ab027b1ed54a41ad4e7e91b4d95`;
- outer/inner radii `15.3 / 10.0`;
- handle centerline `(37.2, 36.3) → (49.0, 48.1)` and radius `3.8`;
- native visible bounds `(10, 9)–(55, 55)`;
- `1,061` native visible pixels and `112` pixels with alpha at least `240`;
- rendered bounds `(7, 7)–(41, 41)`, `624` visible pixels, and `52` pixels with alpha at least `240`;
- unchanged runtime plane and styling.

- [ ] **Step 2: Defer generated RCO/build hashes until Task 5**

Remove the superseded RCO, VELF, ZIP, and profile hashes from the current-build subsection before running fresh builds. Populate that subsection only with exact outputs produced in Task 5.

### Task 5: Run complete offline verification

**Files:**
- Build outputs only under new ignored build directories
- Modify: `docs/firmware/common-corner-back-arrow.md` with fresh exact hashes

- [ ] **Step 1: Run focused and normal host tests**

Configure and run a fresh normal host graph:

```sh
cmake -S . -B build-host-magnifier-geometry
cmake --build build-host-magnifier-geometry --parallel
ctest --test-dir build-host-magnifier-geometry --output-on-failure
```

Expected: all tests pass, including `livearea_search_home_icon_assets`.

- [ ] **Step 2: Run a fresh ASan/UBSan graph**

```sh
cmake -S . -B build-host-magnifier-geometry-sanitized \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-host-magnifier-geometry-sanitized --parallel
ctest --test-dir build-host-magnifier-geometry-sanitized --output-on-failure
```

Expected: all tests pass with no sanitizer report.

- [ ] **Step 3: Run firmware, resource, and source-contract audits**

Use the same firmware roots recorded by the checked-in reports and regenerate temporary reports under `build-analysis/magnifier-geometry-adjustment/`. Require:

```text
213 firmware images passed, 0 failed
84 resources passed, 0 failed
all source-contract checks passed
```

- [ ] **Step 4: Build the permanent release profile**

```sh
LIVEAREA_SEARCH_BUILD_DIR=build-vita-magnifier-geometry-release \
LIVEAREA_SEARCH_DIAGNOSTICS=OFF \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=OFF \
./tools/build-vita.sh
```

Expected: warning-free build and `binary-audit.json` with `passed: true`, `profile: release`, and no issues.

- [ ] **Step 5: Build the self-disabling experimental profile**

```sh
LIVEAREA_SEARCH_BUILD_DIR=build-vita-magnifier-geometry-experimental \
LIVEAREA_SEARCH_DIAGNOSTICS=ON \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=ON \
./tools/build-vita.sh
```

Expected: warning-free build and `binary-audit.json` with `passed: true`, `profile: experimental`, and no issues.

- [ ] **Step 6: Verify RCO payload identity and update documentation**

Require both profiles to contain the same RCO and require the embedded `texture/png` payload to equal `resources/texture/search.png` byte for byte. Record exact RCO, VELF, SUPRX, and release ZIP hashes in `docs/firmware/common-corner-back-arrow.md`.

- [ ] **Step 7: Run whitespace and aggregate diff review**

```sh
git diff --check
git status --short
```

Review the project-scoped aggregate diff. No runtime C++ source, XML geometry, or unrelated file may change.

### Task 6: Commit, push, and export review artifacts

**Files:**
- Modify: `docs/firmware/common-corner-back-arrow.md`
- Export: `resources/texture/search.png`
- Export: `build-vita-magnifier-geometry-release/dist/livearea_search/livearea_search_plugin.rco`

- [ ] **Step 1: Commit final documentation and evidence**

```sh
git add docs/firmware/common-corner-back-arrow.md
git commit -m "Document revised magnifier geometry"
```

- [ ] **Step 2: Push and verify synchronization**

```sh
git push origin main
git fetch origin main
git rev-list --left-right --count origin/main...main
gh api repos/devnoname120/livearea-search/commits/main --jq .sha
```

Expected: divergence `0 0`, remote SHA equals local `HEAD`, and the worktree is clean.

- [ ] **Step 3: Export the revised PNG and RCO**

Provide downloadable copies of:

```text
resources/texture/search.png
build-vita-magnifier-geometry-release/dist/livearea_search/livearea_search_plugin.rco
```

Report their exact SHA-256 values, all offline verification results, and state explicitly that the newly revised proportions have not been deployed to hardware.

# Native Notification Bubble Suppression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the opaque top-right Search occluder with reversible native hide/restore of Sony's audited notification bubble, preserving natural results-header layout.

**Architecture:** `NotificationCornerGuard` will acquire the indicator plugin, its static page, and the native bubble widget by audited hashes for the first suppression and again for restoration. It stores only logical ownership state, issues each native `Hide/Show(Type_Reset)` transition once, removes the plugin-owned occluder resources, and leaves the existing notification-panel ownership state machine unchanged.

**Tech Stack:** C++17, Sony PAF private APIs through the existing compatibility layer, CXML/RCO resources, CMake/CTest contracts, VitaSDK MinSizeRel builds, guarded retail Vita 146 deployment and native capture.

---

## File map

- Modify `include/livearea_search/overlay_visibility.h`: add the audited native notification-bubble hash.
- Modify `src/vita/notification_corner.h`: replace the stored page/widget pointers with logical suppression state and a private widget resolver.
- Modify `src/vita/notification_corner.cpp`: resolve and hide/show Sony's native bubble without retaining its pointer.
- Modify `resources/livearea_search_plugin.xml`: delete the occluder page/styles and restore natural `Recent` alignment.
- Modify `tests/check_notification_corner.cmake`: require the native implementation and forbid the old occluder.
- Modify `tools/firmware/source_firmware_contract.json`: bind the new hash and implementation to firmware evidence.
- Modify `docs/firmware/paf-touchscreen-device-handler.md`: document the newly proven native bubble ID and runtime evidence.
- Create hardware evidence only under ignored `build-vita-regression-diagnostic/` directories.

### Task 1: Lock the intended behavior with failing contracts

**Files:**
- Modify: `tests/check_notification_corner.cmake`
- Modify: `tools/firmware/source_firmware_contract.json`
- Test: `livearea_search_notification_corner`
- Test: source-contract audit

- [ ] **Step 1: Replace the old occluder requirements**

Require the following source markers:

```text
kIndicatorNotificationBubbleHash = 0x718CB7FFU;
paf::ui::Widget* ResolveNativeCorner() const;
bool suppressed_;
bool was_visible_;
paf::Plugin::Find("indicator_plugin")
indicator->PageRoot(kIndicatorPageHash)
root->FindChild(kIndicatorNotificationBubbleHash)
if (suppressed_) {
    return true;
}
native_corner->Hide(paf::common::transition::Type_Reset)
native_corner->Show(paf::common::transition::Type_Reset)
```

Require `text_results_count` to use `pos="0,0"`.

- [ ] **Step 2: Forbid the old implementation**

Reject:

```text
page_notification_occluder
style_notification_bar_occluder
style_notification_header_occluder
overwrite_draw_priority = 7
paf::ui::Scene* page_;
paf::ui::Widget* native_corner_;
pos="-80,0"
```

- [ ] **Step 3: Add firmware/source evidence for `0x718CB7FF`**

Add a source-contract entry requiring the constant in `overlay_visibility.h` and the same hash in `docs/firmware/paf-touchscreen-device-handler.md`.

- [ ] **Step 4: Run the focused contracts and verify RED**

Run:

```sh
cmake -DRESOURCE_XML="$PWD/resources/livearea_search_plugin.xml" \
  -DNOTIFICATION_HEADER="$PWD/src/vita/notification_corner.h" \
  -DNOTIFICATION_SOURCE="$PWD/src/vita/notification_corner.cpp" \
  -DOVERLAY_HEADER="$PWD/src/vita/search_overlay.h" \
  -DSEARCH_OVERLAY="$PWD/src/vita/search_overlay.cpp" \
  -P tests/check_notification_corner.cmake
python3 tools/firmware/source_contract_audit.py \
  --root . \
  --contract tools/firmware/source_firmware_contract.json
```

Expected: both fail because production still uses the plugin-owned occluder and the native hash is absent from the durable source contract.

### Task 2: Implement the minimal production guard

**Files:**
- Modify: `include/livearea_search/overlay_visibility.h`
- Modify: `src/vita/notification_corner.h`
- Modify: `src/vita/notification_corner.cpp`
- Modify: `resources/livearea_search_plugin.xml`
- Modify: `src/vita/search_overlay.cpp`

- [ ] **Step 1: Add the audited bubble hash**

Add:

```cpp
const std::uint32_t kIndicatorNotificationBubbleHash = 0x718CB7FFU;
```

next to the existing indicator page and panel-corner hashes.

- [ ] **Step 2: Replace pointer ownership with logical state**

Use this class shape:

```cpp
class NotificationCornerGuard {
public:
    NotificationCornerGuard();
    ~NotificationCornerGuard();

    bool Suppress();
    bool Restore();

private:
    paf::ui::Widget* ResolveNativeCorner() const;

    bool suppressed_;
    bool was_visible_;
};
```

- [ ] **Step 3: Implement reacquiring hide/restore**

Implement:

```cpp
paf::ui::Widget* NotificationCornerGuard::ResolveNativeCorner() const {
    paf::Plugin* indicator = paf::Plugin::Find("indicator_plugin");
    paf::ui::Scene* root = indicator == NULL ? NULL :
        indicator->PageRoot(kIndicatorPageHash);
    return root == NULL ? NULL :
        root->FindChild(kIndicatorNotificationBubbleHash);
}

bool NotificationCornerGuard::Suppress() {
    if (suppressed_) {
        return true;
    }
    paf::ui::Widget* native_corner = ResolveNativeCorner();
    if (native_corner == NULL) {
        return false;
    }
    was_visible_ = native_corner->IsVisible();
    suppressed_ = true;
    native_corner->Hide(paf::common::transition::Type_Reset);
    return true;
}

bool NotificationCornerGuard::Restore() {
    if (!suppressed_) {
        return true;
    }
    paf::ui::Widget* native_corner = ResolveNativeCorner();
    const bool should_show = was_visible_;
    suppressed_ = false;
    was_visible_ = false;
    if (native_corner == NULL) {
        return true;
    }
    if (should_show) {
        native_corner->Show(paf::common::transition::Type_Reset);
    }
    return true;
}
```

Do not retain the widget pointer or alter its event mask. Repeated calls while
`suppressed_` is true must return without invoking `Hide` again; restarting the
native hide transition every visibility tick leaves the bubble partially visible.

- [ ] **Step 4: Remove the occluder resources and layout reservation**

Delete `page_notification_occluder`, `style_notification_bar_occluder`, and `style_notification_header_occluder`. Set the results-count hint to:

```xml
<layout_hint align="2,1" anchor="2,1" size="150,64" pos="0,0" />
```

- [ ] **Step 5: Remove all diagnostic probe code**

Delete `LogWidgetProbeNode`, `LogPluginWidgetProbe`, transition-triggered tree logging, and autonomous Search opening from `search_overlay.cpp`. Initialize the guard with its parameterless constructor.

- [ ] **Step 6: Run focused contracts and verify GREEN**

Run the two Task 1 commands. Expected: both pass.

### Task 3: Document runtime evidence

**Files:**
- Modify: `docs/firmware/paf-touchscreen-device-handler.md`
- Modify: `docs/superpowers/specs/2026-11-15-native-notification-bubble-suppression-design.md`

- [ ] **Step 1: Record the native bubble structure**

Document page `0xFFD7E3F0`, bubble `0x718CB7FF`, and child IDs `0x6AE5A623`, `0x6E6CC672`, and `0x2D08677D`.

- [ ] **Step 2: Record prototype evidence and restoration**

Reference the prototype capture and the guarded permanent-release restoration report. State that direct `Hide/Show(Type_Reset)` removed the bubble without an occluder or layout reservation.

- [ ] **Step 3: Run source-contract audit again**

Expected: all checks pass.

### Task 4: Run complete offline verification

**Files:**
- Outputs only under fresh ignored build directories

- [ ] **Step 1: Fresh normal host graph**

```sh
cmake -S . -B build-host-native-notification
cmake --build build-host-native-notification --parallel
ctest --test-dir build-host-native-notification --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 2: Fresh ASan/UBSan graph**

```sh
cmake -S . -B build-host-native-notification-sanitized \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-host-native-notification-sanitized --parallel
ctest --test-dir build-host-native-notification-sanitized --output-on-failure
```

Expected: all tests pass without sanitizer diagnostics.

- [ ] **Step 3: Firmware/resource/source audits**

Run the existing audit tools against the recorded main and retail-reference roots. Require aggregate firmware `213/213`, resources `84/84`, and all source contracts green.

- [ ] **Step 4: Build warning-free Vita profiles**

Build:

```sh
LIVEAREA_SEARCH_BUILD_DIR=build-vita-native-notification-release \
LIVEAREA_SEARCH_BUILD_TYPE=MinSizeRel \
LIVEAREA_SEARCH_DIAGNOSTICS=OFF \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=OFF \
./tools/build-vita.sh

LIVEAREA_SEARCH_BUILD_DIR=build-vita-native-notification-experimental \
LIVEAREA_SEARCH_BUILD_TYPE=MinSizeRel \
LIVEAREA_SEARCH_DIAGNOSTICS=ON \
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=ON \
./tools/build-vita.sh
```

Require both binary audits to pass and both RCO files to be byte-identical.

### Task 5: Guarded hardware qualification

**Files:**
- Create ignored deployment/capture scripts under `build-vita-regression-diagnostic/`

- [ ] **Step 1: Deploy only the self-disabling experimental candidate**

Preserve exact permanent SUPRX/RCO/config/startup/core fingerprints, stage disabled config first, upload and read back candidate files, activate config last, and cold-reset through the proven helper. Require post-start `(0 active, 1 disabled)`, exact hashes, overlay-active diagnostics, and unchanged core names plus fixed-name fingerprints.

- [ ] **Step 2: Capture Search open/close visually**

Require:

- native bubble absent while Search is open;
- no black rectangle or cyan remnant;
- `Recent` aligned at the natural right edge;
- bubble restored after closing Search;
- no hidden active corner stealing input.

- [ ] **Step 3: Run ownership regressions**

Run notification-panel, switcher, PS/Home active/dismissed IME, Start/Home-edit/external-IME, and Triangle/Circle sequences using existing validated harnesses. Require exact candidate/config readback and no cores after every sequence.

- [ ] **Step 4: Install and verify the matching release**

Only after the exact candidate passes, install the diagnostics-off release, activate one config entry, cold-reset twice, run Search/Back smoke, and require stable startup-log bytes plus unchanged core fingerprints.

### Task 6: Final review and repository completion

**Files:**
- All tracked files above

- [ ] **Step 1: Review aggregate diff**

Run:

```sh
git diff --check
git status --short
git diff --stat
```

Confirm no diagnostic probe code, occluder resource, release tag, or publication artifact is tracked.

- [ ] **Step 2: Commit and push**

```sh
git add \
  include/livearea_search/overlay_visibility.h \
  src/vita/notification_corner.h \
  src/vita/notification_corner.cpp \
  src/vita/search_overlay.cpp \
  resources/livearea_search_plugin.xml \
  tests/check_notification_corner.cmake \
  tools/firmware/source_firmware_contract.json \
  docs/firmware/paf-touchscreen-device-handler.md \
  docs/superpowers/specs/2026-11-15-native-notification-bubble-suppression-design.md \
  docs/superpowers/plans/2026-11-15-native-notification-bubble-suppression.md
git commit -m "Hide native notification bubble during Search"
git push origin main
```

- [ ] **Step 3: Verify synchronization**

Require local `main`, `origin/main`, and GitHub `main` to resolve to the same SHA, divergence `0 0`, and a clean worktree. Confirm no release or tag was created.

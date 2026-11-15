# Results Back-Corner Close Transition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Preserve Sony's native 150 ms bottom-left Back-corner hide animation before ordinary Search result-page teardown while keeping safety closes immediate and PS/Home ownership leak-free.

**Architecture:** Add a deterministic core close-transition state machine that separates callback-safe request queuing from main-thread animation/teardown actions. `SearchOverlay` disables all results input when a close is queued, starts `CornerButton::Hide(Type_Reset)` once for ordinary closes, retains the results page and PS lock until the firmware-derived deadline, and routes app-launch/system/failure/unload paths through next-tick immediate teardown.

**Tech Stack:** C++11 core policy, Sony PAF private widget APIs through the existing compatibility layer, CMake/CTest source contracts, VitaSDK MinSizeRel builds, guarded retail Vita 146 deployment and native capture.

---

## File structure

- Create `include/livearea_search/results_close_transition.h`: close modes, states, actions, and public policy API.
- Create `src/core/results_close_transition.cpp`: deterministic request/advance state machine using `kHomeCornerHideDurationUs`.
- Create `tests/test_results_close_transition.cpp`: isolated timing, idempotence, and escalation tests.
- Modify `CMakeLists.txt`: compile the new core policy.
- Modify `tests/CMakeLists.txt`: register the policy test and pass policy/header paths into close contracts.
- Modify `include/livearea_search/overlay_visibility.h`, `src/core/overlay_visibility.cpp`, and `tests/test_app_index.cpp`: remove the obsolete one-frame-only deferred-close policy.
- Modify `src/vita/search_overlay.h` and `src/vita/search_overlay.cpp`: integrate animated and immediate requests, input quiescence, deadline processing, and PS-lock retention.
- Modify `tests/check_close_lifecycle.cmake` and `tests/check_search_polish.cmake`: enforce source ordering and ownership invariants.
- Modify `docs/firmware/home-corner-native-transition.md`: record results Back reuse and hardware evidence.
- Modify `tools/firmware/source_firmware_contract.json` and regenerate source-contract reports if the new production contract needs durable audit coverage.

### Task 1: Add the deterministic results-close policy

**Files:**
- Create: `include/livearea_search/results_close_transition.h`
- Create: `src/core/results_close_transition.cpp`
- Create: `tests/test_results_close_transition.cpp`
- Modify: `CMakeLists.txt:18-29`
- Modify: `tests/CMakeLists.txt:88-114`

- [x] **Step 1: Write the failing policy test**

Create `tests/test_results_close_transition.cpp` with the following cases:

```cpp
#include <livearea_search/results_close_transition.h>

#include <cstdint>
#include <cstdio>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
} // namespace

int main() {
    using livearea_search::AdvanceResultsCloseTransition;
    using livearea_search::QueueResultsClose;
    using livearea_search::ResultsCloseAction;
    using livearea_search::ResultsCloseMode;
    using livearea_search::ResultsCloseState;
    using livearea_search::kResultsBackHideDurationUs;

    ResultsCloseState state = ResultsCloseState::Idle;
    std::uint64_t deadline_us = 0;
    const std::uint64_t start_us = 1000000;

    QueueResultsClose(ResultsCloseMode::AnimatedBackCorner, state, deadline_us);
    Check(state == ResultsCloseState::PendingAnimated && deadline_us == 0,
          "ordinary close queues without running inside the callback");
    Check(AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
              ResultsCloseAction::StartBackCornerHide &&
          state == ResultsCloseState::AnimatingBackCorner &&
          deadline_us == start_us + kResultsBackHideDurationUs,
          "the first main-thread tick starts one native hide interval");

    QueueResultsClose(ResultsCloseMode::AnimatedBackCorner, state, deadline_us);
    Check(deadline_us == start_us + kResultsBackHideDurationUs,
          "repeated ordinary requests do not restart the deadline");
    Check(AdvanceResultsCloseTransition(start_us + kResultsBackHideDurationUs - 1,
                                        state,
                                        deadline_us) ==
              ResultsCloseAction::None,
          "teardown waits for the complete firmware hide duration");
    Check(AdvanceResultsCloseTransition(start_us + kResultsBackHideDurationUs,
                                        state,
                                        deadline_us) ==
              ResultsCloseAction::Close &&
          state == ResultsCloseState::Idle && deadline_us == 0,
          "teardown runs at the hide deadline and resets state");

    QueueResultsClose(ResultsCloseMode::Immediate, state, deadline_us);
    Check(state == ResultsCloseState::PendingImmediate,
          "safety close is queued for callback-safe teardown");
    Check(AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
              ResultsCloseAction::Close &&
          state == ResultsCloseState::Idle,
          "safety close executes on the next main-thread tick");

    QueueResultsClose(ResultsCloseMode::AnimatedBackCorner, state, deadline_us);
    Check(AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
              ResultsCloseAction::StartBackCornerHide,
          "a second ordinary close can begin after reset");
    QueueResultsClose(ResultsCloseMode::Immediate, state, deadline_us);
    Check(state == ResultsCloseState::PendingImmediate && deadline_us == 0,
          "safety ownership preempts an active animation");
    Check(AdvanceResultsCloseTransition(start_us + 1, state, deadline_us) ==
              ResultsCloseAction::Close,
          "preempted animation closes on the next tick");

    return failures == 0 ? 0 : 1;
}
```

- [x] **Step 2: Register the missing test and verify RED**

Add the executable and CTest registration to `tests/CMakeLists.txt`, then run:

```sh
cmake -S . -B build-host-results-close-red \
  -DLIVEAREA_SEARCH_BUILD_TESTS=ON \
  -DLIVEAREA_SEARCH_BUILD_VITA=OFF
cmake --build build-host-results-close-red --target livearea_search_results_close_transition_tests
```

Expected: compilation fails because `livearea_search/results_close_transition.h` does not exist.

- [x] **Step 3: Add the policy header**

Create `include/livearea_search/results_close_transition.h`:

```cpp
#pragma once

#include <cstdint>

#include <livearea_search/home_corner_transition.h>

namespace livearea_search {

constexpr std::uint64_t kResultsBackHideDurationUs =
    kHomeCornerHideDurationUs;

enum class ResultsCloseMode {
    AnimatedBackCorner,
    Immediate,
};

enum class ResultsCloseState {
    Idle,
    PendingAnimated,
    AnimatingBackCorner,
    PendingImmediate,
};

enum class ResultsCloseAction {
    None,
    StartBackCornerHide,
    Close,
};

void QueueResultsClose(
    ResultsCloseMode mode,
    ResultsCloseState& state,
    std::uint64_t& deadline_us) noexcept;

ResultsCloseAction AdvanceResultsCloseTransition(
    std::uint64_t now_us,
    ResultsCloseState& state,
    std::uint64_t& deadline_us) noexcept;

} // namespace livearea_search
```

- [x] **Step 4: Add the minimal state-machine implementation**

Create `src/core/results_close_transition.cpp`:

```cpp
#include <livearea_search/results_close_transition.h>

namespace livearea_search {

void QueueResultsClose(
    ResultsCloseMode mode,
    ResultsCloseState& state,
    std::uint64_t& deadline_us) noexcept {
    if (mode == ResultsCloseMode::Immediate) {
        state = ResultsCloseState::PendingImmediate;
        deadline_us = 0;
        return;
    }
    if (state == ResultsCloseState::Idle) {
        state = ResultsCloseState::PendingAnimated;
        deadline_us = 0;
    }
}

ResultsCloseAction AdvanceResultsCloseTransition(
    std::uint64_t now_us,
    ResultsCloseState& state,
    std::uint64_t& deadline_us) noexcept {
    if (state == ResultsCloseState::PendingImmediate) {
        state = ResultsCloseState::Idle;
        deadline_us = 0;
        return ResultsCloseAction::Close;
    }
    if (state == ResultsCloseState::PendingAnimated) {
        state = ResultsCloseState::AnimatingBackCorner;
        deadline_us = now_us + kResultsBackHideDurationUs;
        return ResultsCloseAction::StartBackCornerHide;
    }
    if (state == ResultsCloseState::AnimatingBackCorner &&
        deadline_us != 0 && now_us >= deadline_us) {
        state = ResultsCloseState::Idle;
        deadline_us = 0;
        return ResultsCloseAction::Close;
    }
    return ResultsCloseAction::None;
}

} // namespace livearea_search
```

Add `src/core/results_close_transition.cpp` to `livearea_search_core`.

- [x] **Step 5: Run the focused test and verify GREEN**

```sh
cmake --build build-host-results-close-red --target livearea_search_results_close_transition_tests
ctest --test-dir build-host-results-close-red \
  -R livearea_search_results_close_transition_tests \
  --output-on-failure
```

Expected: 1/1 passed.

- [x] **Step 6: Commit the isolated policy**

```sh
git add CMakeLists.txt tests/CMakeLists.txt \
  include/livearea_search/results_close_transition.h \
  src/core/results_close_transition.cpp \
  tests/test_results_close_transition.cpp
git commit -m "Add results close transition policy"
```

### Task 2: Remove the obsolete frame-counter close policy

**Files:**
- Modify: `include/livearea_search/overlay_visibility.h:31-35,178-180`
- Modify: `src/core/overlay_visibility.cpp:336-346`
- Modify: `tests/test_app_index.cpp:591-618,729`

- [x] **Step 1: Remove `DeferredResultCloseAction` and `AdvanceDeferredResultClose` declarations**

Delete the old enum and function declaration from `overlay_visibility.h`; the new dedicated policy owns all close timing.

- [x] **Step 2: Remove the old implementation**

Delete `AdvanceDeferredResultClose` from `src/core/overlay_visibility.cpp`.

- [x] **Step 3: Remove the obsolete unit test**

Delete `TestDeferredResultClosePolicy()` and its `main()` call from `tests/test_app_index.cpp`.

- [x] **Step 4: Build the two affected host targets**

```sh
cmake --build build-host-results-close-red --target \
  livearea_search_tests \
  livearea_search_results_close_transition_tests
ctest --test-dir build-host-results-close-red \
  -R 'livearea_search_(tests|results_close_transition_tests)' \
  --output-on-failure
```

Expected: both tests pass.

- [x] **Step 5: Commit the policy replacement**

```sh
git add include/livearea_search/overlay_visibility.h \
  src/core/overlay_visibility.cpp tests/test_app_index.cpp
git commit -m "Replace deferred results close policy"
```

### Task 3: Add RED source contracts for animated teardown

**Files:**
- Modify: `tests/check_close_lifecycle.cmake`
- Modify: `tests/check_search_polish.cmake`
- Modify: `tests/CMakeLists.txt:271-278`

- [x] **Step 1: Extend the close-lifecycle contract inputs**

Pass `POLICY_HEADER`, `OVERLAY_HEADER`, and `HOME_TRANSITION_HEADER` to `check_close_lifecycle.cmake`.

- [x] **Step 2: Require the new state, modes, input quiescence, and ordering**

Add checks for these exact source contracts:

```cmake
"ResultsCloseState results_close_state_;"
"std::uint64_t results_close_deadline_us_;"
"void RequestCloseResultsPage(ResultsCloseMode mode);"
"void DisableResultsInput();"
"RequestCloseResultsPage(ResultsCloseMode::AnimatedBackCorner);"
"RequestCloseResultsPage(ResultsCloseMode::Immediate);"
"close_button_->Hide(paf::common::transition::Type_Reset);"
"AdvanceResultsCloseTransition("
"kResultsBackHideDurationUs"
```

Isolate `UpdateHomePadClose()` and require that it queues close without calling `ReleaseHomePadLock()`. Isolate `CloseResultsPage()` and require `ReleaseHomePadLock()` before `PageClose`. Require the hide call to appear before the close call in the lifecycle source.

Require `DisableResultsInput()` to disable the full mask on `results_page_`, `search_box_`, `result_list_`, and `close_button_`, and to release focus on focusable controls.

- [x] **Step 3: Update the PS/Home polish contract**

Replace the former `ReleaseLockAndClose -> unlock -> RequestCloseResultsPage()` expectation with `CloseAfterRelease` and:

```cmake
"RequestCloseResultsPage(\n        results_editing_layout_ ?\n            ResultsCloseMode::Immediate :\n            ResultsCloseMode::AnimatedBackCorner);"
```

Forbid `ReleaseHomePadLock();` inside `UpdateHomePadClose()` and retain the existing contract that `HandleHome()` cannot close before physical release.

- [x] **Step 4: Run the contracts and verify RED**

```sh
ctest --test-dir build-host-results-close-red \
  -R 'livearea_search_(close_lifecycle|polish_contract)' \
  --output-on-failure
```

Expected: both contracts fail because production still has `close_requested_`, `close_defer_frames_`, no explicit close mode, and unlocks before close.

- [x] **Step 5: Commit the RED contracts**

```sh
git add tests/CMakeLists.txt tests/check_close_lifecycle.cmake \
  tests/check_search_polish.cmake
git commit -m "Test results Back close animation"
```

### Task 4: Integrate the transition into `SearchOverlay`

**Files:**
- Modify: `src/vita/search_overlay.h:7-10,116-118,171-173`
- Modify: `src/vita/search_overlay.cpp:404-451,626-710,837-870,953-1054,1184-1188,1235-1240,1287-1297,1494-1498`

- [x] **Step 1: Replace close fields and method declarations**

Include `results_close_transition.h`, replace `close_requested_` / `close_defer_frames_` with:

```cpp
ResultsCloseState results_close_state_;
std::uint64_t results_close_deadline_us_;
```

Use these declarations:

```cpp
void RequestCloseResultsPage(ResultsCloseMode mode);
void DisableResultsInput();
void ProcessCloseRequest();
void CloseResultsPage();
```

- [x] **Step 2: Initialize and reset the new state**

Initialize `results_close_state_` to `ResultsCloseState::Idle` and the deadline to `0`. Reset both when opening a fresh results page and in every `CloseResultsPage()` path.

- [x] **Step 3: Add complete input quiescence**

Implement:

```cpp
void SearchOverlay::DisableResultsInput() {
    const std::uint32_t mask =
        paf::ui::EV_TOUCH |
        paf::ui::EV_GESTURE_ALL |
        paf::ui::EV_KEYCODE |
        paf::ui::EV_ANALOG |
        paf::ui::EV_FOCUS;
    if (results_page_ != NULL) {
        results_page_->DisableEvent(mask);
        results_page_->ReleaseFocus();
    }
    if (search_box_ != NULL) {
        search_box_->DisableEvent(mask);
        search_box_->ReleaseFocus();
    }
    if (result_list_ != NULL) {
        result_list_->DisableEvent(mask);
        result_list_->ReleaseFocus();
    }
    if (close_button_ != NULL) {
        close_button_->DisableEvent(mask);
        close_button_->ReleaseFocus();
    }
}
```

- [x] **Step 4: Queue explicit animated or immediate closes**

Implement:

```cpp
void SearchOverlay::RequestCloseResultsPage(ResultsCloseMode mode) {
    if (results_page_ == NULL) {
        return;
    }
    DisableResultsInput();
    UpdateHomeEditLock(false);
    QueueResultsClose(mode, results_close_state_, results_close_deadline_us_);
}
```

Call animated mode only from `HandleBack()` and from `UpdateHomePadClose()` when `results_editing_layout_` is false. Call immediate mode from BeginEdit failure, IME cancellation/empty submission, edit-lock failure, shell visibility loss, app launch, and any other non-user safety request.

- [x] **Step 5: Retain the PS lock through animation**

Change `UpdateHomePadClose()` to:

```cpp
void SearchOverlay::UpdateHomePadClose() {
    const HomePadCloseAction action = ComputeHomePadCloseAction(
        home_close_pending_,
        home_ps_owned_);
    if (action != HomePadCloseAction::CloseAfterRelease) {
        return;
    }
    RequestCloseResultsPage(
        results_editing_layout_ ?
            ResultsCloseMode::Immediate :
            ResultsCloseMode::AnimatedBackCorner);
}
```

Do not unlock here. `CloseResultsPage()` remains the single final PS-unlock location.

- [x] **Step 6: Start one native hide and close at the deadline**

Implement:

```cpp
void SearchOverlay::ProcessCloseRequest() {
    const ResultsCloseAction action = AdvanceResultsCloseTransition(
        static_cast<std::uint64_t>(sceKernelGetSystemTimeWide()),
        results_close_state_,
        results_close_deadline_us_);
    if (action == ResultsCloseAction::StartBackCornerHide) {
        if (close_button_ == NULL || !close_button_->IsVisible()) {
            QueueResultsClose(
                ResultsCloseMode::Immediate,
                results_close_state_,
                results_close_deadline_us_);
            return;
        }
        QueueInteractionLog("results Back hide begin");
        close_button_->Hide(paf::common::transition::Type_Reset);
        return;
    }
    if (action == ResultsCloseAction::Close) {
        QueueInteractionLog("results close transition complete");
        CloseResultsPage();
    }
}
```

- [x] **Step 7: Run focused contracts and host tests**

```sh
cmake --build build-host-results-close-red
ctest --test-dir build-host-results-close-red \
  -R 'livearea_search_(results_close_transition_tests|tests|close_lifecycle|polish_contract)' \
  --output-on-failure
```

Expected: all selected tests pass.

- [x] **Step 8: Commit runtime integration**

```sh
git add src/vita/search_overlay.h src/vita/search_overlay.cpp \
  tests/check_close_lifecycle.cmake tests/check_search_polish.cmake
git commit -m "Animate results Back before close"
```

### Task 5: Update durable contracts and documentation

**Files:**
- Modify: `docs/firmware/home-corner-native-transition.md`
- Modify: `tools/firmware/source_firmware_contract.json`
- Regenerate: `docs/firmware/source-contract-report.json`
- Regenerate: `docs/firmware/source-contract-report.md`
- Modify: `docs/superpowers/specs/2026-11-15-results-back-corner-close-transition-design.md`
- Modify: `docs/superpowers/plans/2026-11-15-results-back-corner-close-transition.md`

- [x] **Step 1: Add the results Back reuse evidence section**

Document the state machine, exact 150 ms duration, input-disable ordering, and PS-lock retention in `home-corner-native-transition.md`.

- [x] **Step 2: Add source-to-firmware needles**

Require the new results-close header constant alias, `Hide(Type_Reset)`, the 150 ms firmware evidence, and final-teardown PS unlock ordering in `source_firmware_contract.json`.

- [x] **Step 3: Regenerate and verify the source reports**

```sh
python3 tools/firmware/source_contract_audit.py \
  --root . \
  --contract tools/firmware/source_firmware_contract.json \
  --json-out docs/firmware/source-contract-report.json \
  --markdown-out docs/firmware/source-contract-report.md
```

Expected: every source contract passes and both checked-in reports are updated.

- [x] **Step 4: Mark completed plan steps and append exact final evidence after it exists**

Use exact hashes and evidence directories from Tasks 6-7; do not invent them.

- [x] **Step 5: Commit durable offline evidence**

```sh
git add docs/firmware/home-corner-native-transition.md \
  tools/firmware/source_firmware_contract.json \
  docs/firmware/source-contract-report.json \
  docs/firmware/source-contract-report.md \
  docs/superpowers/specs/2026-11-15-results-back-corner-close-transition-design.md \
  docs/superpowers/plans/2026-11-15-results-back-corner-close-transition.md
git commit -m "Document results Back close lifecycle"
```

### Task 6: Run full offline verification and build exact Vita artifacts

**Files:**
- Generated/ignored: fresh host build directories, audit receipts, Vita release and experimental build directories.

- [x] **Step 1: Run a fresh normal host graph**

Configure, build, and run all tests. Expected: every test passes with no compiler warnings.

- [x] **Step 2: Run a fresh ASan+UBSan host graph**

Configure with the repository's sanitizer flags, build, and run all tests. Expected: every test passes with no sanitizer finding.

- [x] **Step 3: Run firmware, resource, and source audits**

Require the established totals: firmware `203 + 10 = 213`, resources `84`, and all source contracts passing.

- [x] **Step 4: Build diagnostics-off and self-disabling MinSizeRel artifacts**

Use `deploy/build_vita.sh` with distinct build directories and:

```sh
LIVEAREA_SEARCH_BUILD_TYPE=MinSizeRel
LIVEAREA_SEARCH_DIAGNOSTICS=OFF
LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=OFF
```

for release, and matching `ON/ON` values for the experimental profile. Require both binary audits to pass and both RCO files to be byte-identical.

- [x] **Step 5: Record SHA-256 values and verify no diagnostic strings in release**

Hash VELF, SUPRX, RCO, and ZIP. Search the release binary for startup-log, TTY, self-disable, and config-mutation strings; require none.

### Task 7: Guarded retail Vita 146 qualification

**Files:**
- Generated/ignored: `build-vita-regression-diagnostic/results-back-close-*` evidence directories and harnesses.

- [x] **Step 1: Deploy only the exact self-disabling candidate**

Preserve the current permanent SUPRX/RCO/config/startup/core baseline, stage exact candidate bytes, read them back, activate last, reboot with outage monitoring, and require config `0 active / 1 disabled` after startup.

- [x] **Step 2: Capture Circle with keyboard hidden**

Open Search, dismiss IME, press Circle, and capture enough native frames to prove the Back corner shrinks before the results page disappears. Require approximately 150 ms between hide start and page teardown, native notification restoration afterward, exact hashes/config, and no core changes.

- [x] **Step 3: Capture tap-Back**

Repeat using the bottom-left touch target and require the same visible sequence.

- [x] **Step 4: Capture PS/Home with keyboard hidden**

Require physical PS release before hide start, PS lock held through the hide interval, unlock during final teardown, no crossed-circle indicator, no Home overview/application switcher, and no core changes.

- [x] **Step 5: Regression-test forced paths and ownership matrix**

Run app launch, notification panel, switcher, Start/edit/external-IME, Triangle/Circle, and active-IME PS coverage. Immediate safety paths must not retain stale UI.

### Task 8: Permanently install, final-verify, commit, and push

**Files:**
- Modify final evidence sections in the design, plan, and firmware report.

- [x] **Step 1: Guarded-install the matching release**

Install only after Task 7 passes. Require one active and zero disabled config entries, exact readback, two clean boots, Search/Back smoke, stable release startup bytes, and unchanged core fingerprints.

- [x] **Step 2: Capture final diagnostics-off Circle and PS/Home evidence**

Confirm the same Back-corner animation in the permanent release.

- [x] **Step 3: Rerun fresh verification after final documentation changes**

Run normal tests, sanitizer tests, all audits, binary audits, and `git diff --check` again.

- [x] **Step 4: Present the aggregate diff**

Run the project diff review and verify no diagnostic probes, generated evidence, or unrelated changes are tracked.

- [x] **Step 5: Commit final evidence and push**

```sh
git add \
  CMakeLists.txt \
  tests/CMakeLists.txt \
  include/livearea_search/results_close_transition.h \
  include/livearea_search/overlay_visibility.h \
  src/core/results_close_transition.cpp \
  src/core/overlay_visibility.cpp \
  src/vita/search_overlay.h \
  src/vita/search_overlay.cpp \
  tests/test_results_close_transition.cpp \
  tests/test_app_index.cpp \
  tests/check_close_lifecycle.cmake \
  tests/check_search_polish.cmake \
  docs/firmware/home-corner-native-transition.md \
  tools/firmware/source_firmware_contract.json \
  docs/firmware/source-contract-report.json \
  docs/firmware/source-contract-report.md \
  docs/superpowers/specs/2026-11-15-results-back-corner-close-transition-design.md \
  docs/superpowers/plans/2026-11-15-results-back-corner-close-transition.md
git commit -m "Animate results Back corner on close"
git push origin main
```

- [x] **Step 6: Independently verify completion**

Fetch `origin`, query GitHub through `gh`, and require local `main`, `origin/main`, and GitHub `main` to match with divergence `0/0` and a clean worktree. Confirm no new tag or GitHub release exists.

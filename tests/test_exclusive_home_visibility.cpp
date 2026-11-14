#include <livearea_search/overlay_visibility.h>

#include <cstdio>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

} // namespace

int main() {
    using livearea_search::ComputeHomeVisibilityAction;
    using livearea_search::HomeVisibilityAction;
    using livearea_search::IsHomeBackCornerWidget;
    using livearea_search::IsHomeEditOverlayWidget;
    using livearea_search::IsHomeEditScrollWidget;
    using livearea_search::ShouldExposeHomeSearch;

    const bool edit_overlay =
        IsHomeEditOverlayWidget(true, true, 0.0f, 0.0f, 1000.0f, 600.0f);
    const bool back_corner =
        IsHomeBackCornerWidget(true, true, -480.0f, -272.0f, 90.0f, 90.0f);
    const bool edit_scroll =
        IsHomeEditScrollWidget(true, true, 0.0f, 0.0f, 960.0f, 544.0f);
    Check(edit_overlay && edit_scroll && back_corner,
          "the hardware-observed edit overlay, full-screen scroll, and Back corner identify edit mode");
    Check(!IsHomeEditOverlayWidget(
              true, true, 0.0f, 0.0f, 959.0f, 600.0f),
          "a partial overlay must not be mistaken for Home edit mode");
    Check(!IsHomeEditOverlayWidget(
              false, true, 0.0f, 0.0f, 1000.0f, 600.0f),
          "a hidden full-screen widget must not claim edit ownership");
    Check(!IsHomeBackCornerWidget(
              true, true, 0.0f, -272.0f, 90.0f, 90.0f),
          "a non-corner button must not identify the native Back control");
    Check(!IsHomeEditScrollWidget(
              true, true, 0.0f, -16.0f, 960.0f, 512.0f) && back_corner,
          "ordinary folder view may expose Back without the full-screen edit scroll");
    Check(!IsHomeEditScrollWidget(
              true, false, 0.0f, 0.0f, 960.0f, 544.0f),
          "a full-screen non-scroll widget must not claim persistent edit ownership");

    Check(
        ShouldExposeHomeSearch(true, true, false, false, false, false),
        "bubble Home must expose Search when no higher shell surface is visible");
    Check(
        !ShouldExposeHomeSearch(true, true, false, true, false, false),
        "a visible LiveArea card must own the foreground and hide Search");
    Check(
        !ShouldExposeHomeSearch(true, true, false, false, true, false),
        "an external IME must own the lower-right keyboard region");
    Check(
        !ShouldExposeHomeSearch(true, true, false, false, false, true),
        "Home edit controls must own the lower-right page controls");

    Check(
        ComputeHomeVisibilityAction(true, false, false, false) ==
            HomeVisibilityAction::HideWithFade,
        "ordinary Home departure must use the native fade-out");
    Check(
        ComputeHomeVisibilityAction(true, false, false, true) ==
            HomeVisibilityAction::HideImmediately,
        "system-owned controls must suppress Search immediately");
    Check(
        ComputeHomeVisibilityAction(true, true, true, false) ==
            HomeVisibilityAction::HideImmediately,
        "opening Search results must remove the Home control immediately");
    Check(
        ComputeHomeVisibilityAction(false, true, false, false) ==
            HomeVisibilityAction::Show,
        "returning to ordinary Home must retain the native fade-in");

    return failures == 0 ? 0 : 1;
}

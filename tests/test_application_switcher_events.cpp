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
    using livearea_search::ApplicationSwitcherEventAction;
    using livearea_search::ClassifyApplicationSwitcherEvent;
    using livearea_search::ComputeHomeVisibilityAction;
    using livearea_search::HomeVisibilityAction;
    using livearea_search::ShouldExposeHomeSearch;
    using livearea_search::kShellEventApplicationSwitcherEnter;
    using livearea_search::kShellEventApplicationSwitcherLeave;

    Check(
        ClassifyApplicationSwitcherEvent(
            kShellEventApplicationSwitcherEnter) ==
            ApplicationSwitcherEventAction::Enter,
        "shell event 4 must begin application-switcher ownership");
    Check(
        ClassifyApplicationSwitcherEvent(
            kShellEventApplicationSwitcherLeave) ==
            ApplicationSwitcherEventAction::Leave,
        "shell event 5 must end application-switcher ownership");
    Check(
        ClassifyApplicationSwitcherEvent(3) ==
            ApplicationSwitcherEventAction::None &&
        ClassifyApplicationSwitcherEvent(6) ==
            ApplicationSwitcherEventAction::None,
        "unrelated shell events must not change switcher ownership");

    const bool exposed_while_switching = ShouldExposeHomeSearch(
        true,
        true,
        false,
        false,
        false,
        false,
        true);
    Check(
        !exposed_while_switching,
        "the application switcher must own the Home corner");
    Check(
        ComputeHomeVisibilityAction(
            true,
            exposed_while_switching,
            false,
            false) == HomeVisibilityAction::HideWithFade,
        "switcher entry must use the native fast fade-out");

    const bool exposed_after_return = ShouldExposeHomeSearch(
        true,
        true,
        false,
        false,
        false,
        false,
        false);
    Check(
        exposed_after_return,
        "switcher exit must restore ordinary Home eligibility");
    Check(
        ComputeHomeVisibilityAction(
            false,
            exposed_after_return,
            false,
            false) == HomeVisibilityAction::Show,
        "returning Home must use the native fast fade-in");

    Check(
        ComputeHomeVisibilityAction(
            true,
            false,
            false,
            true) == HomeVisibilityAction::HideImmediately,
        "system-owned surfaces must retain hard-hide precedence");

    return failures == 0 ? 0 : 1;
}

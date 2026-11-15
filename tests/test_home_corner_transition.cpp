#include <livearea_search/home_corner_transition.h>

#include <cstdint>
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
    using livearea_search::HomeCornerTransitionAction;
    using livearea_search::HomeCornerTransitionCompletion;
    using livearea_search::HomeCornerTransitionState;
    using livearea_search::AdvanceHomeCornerTransition;
    using livearea_search::RequestHomeCornerTransition;
    using livearea_search::kHomeCornerHideDurationUs;
    using livearea_search::kHomeCornerShowDurationUs;

    HomeCornerTransitionState state = HomeCornerTransitionState::Hidden;
    std::uint64_t deadline_us = 0;
    const std::uint64_t start_us = 1000000;
    Check(
        RequestHomeCornerTransition(
            true,
            false,
            start_us,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::ShowNative &&
        state == HomeCornerTransitionState::Showing &&
        deadline_us == start_us + kHomeCornerShowDurationUs,
        "hidden Search must start the native CornerButton show transition");
    Check(
        RequestHomeCornerTransition(
            true,
            false,
            start_us + 1000,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::None &&
        state == HomeCornerTransitionState::Showing &&
        deadline_us == start_us + kHomeCornerShowDurationUs,
        "repeated visible updates must not restart the native show transition");
    Check(
        AdvanceHomeCornerTransition(
            start_us + 100000,
            state,
            deadline_us) == HomeCornerTransitionCompletion::None &&
        state == HomeCornerTransitionState::Showing,
        "the show transition must remain active until its firmware duration expires");

    const std::uint64_t reverse_hide_us = start_us + 300000;
    Check(
        RequestHomeCornerTransition(
            false,
            false,
            reverse_hide_us,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::HideNative &&
        state == HomeCornerTransitionState::Hiding &&
        deadline_us == reverse_hide_us + kHomeCornerHideDurationUs,
        "a hide request during show must reverse through the native hide path");
    Check(
        AdvanceHomeCornerTransition(
            start_us + kHomeCornerShowDurationUs,
            state,
            deadline_us) == HomeCornerTransitionCompletion::None &&
        state == HomeCornerTransitionState::Hiding,
        "the superseded show deadline must not revive a reversing corner");
    Check(
        RequestHomeCornerTransition(
            false,
            false,
            reverse_hide_us + 1000,
            state,
            deadline_us) == HomeCornerTransitionAction::None &&
        deadline_us == reverse_hide_us + kHomeCornerHideDurationUs,
        "repeated hidden updates must not restart the native hide transition");

    const std::uint64_t reverse_show_us = reverse_hide_us + 50000;
    Check(
        RequestHomeCornerTransition(
            true,
            false,
            reverse_show_us,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::ShowNative &&
        state == HomeCornerTransitionState::Showing &&
        deadline_us == reverse_show_us + kHomeCornerShowDurationUs,
        "a show request during hide must replace the hide transition");
    Check(
        AdvanceHomeCornerTransition(
            reverse_hide_us + kHomeCornerHideDurationUs,
            state,
            deadline_us) == HomeCornerTransitionCompletion::None &&
        state == HomeCornerTransitionState::Showing,
        "the superseded hide deadline must not inactivate a reversing corner");
    Check(
        AdvanceHomeCornerTransition(
            reverse_show_us + kHomeCornerShowDurationUs,
            state,
            deadline_us) == HomeCornerTransitionCompletion::Show &&
        state == HomeCornerTransitionState::Visible &&
        deadline_us == 0,
        "the active show deadline must settle the corner as visible");

    const std::uint64_t ordinary_hide_us = reverse_show_us + 500000;
    Check(
        RequestHomeCornerTransition(
            false,
            false,
            ordinary_hide_us,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::HideNative &&
        state == HomeCornerTransitionState::Hiding &&
        deadline_us == ordinary_hide_us + kHomeCornerHideDurationUs,
        "ordinary Home departure must use the native corner hide transition");
    Check(
        AdvanceHomeCornerTransition(
            ordinary_hide_us + kHomeCornerHideDurationUs,
            state,
            deadline_us) == HomeCornerTransitionCompletion::Hide &&
        state == HomeCornerTransitionState::Hidden &&
        deadline_us == 0,
        "the active hide deadline must settle the corner as hidden");

    Check(
        RequestHomeCornerTransition(
            false,
            true,
            ordinary_hide_us + 1000000,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::None &&
        state == HomeCornerTransitionState::Hidden &&
        deadline_us == 0,
        "an already hidden corner must ignore repeated immediate suppression");

    state = HomeCornerTransitionState::Visible;
    deadline_us = ordinary_hide_us + 2000000;
    Check(
        RequestHomeCornerTransition(
            false,
            true,
            ordinary_hide_us + 1100000,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::HideImmediately &&
        state == HomeCornerTransitionState::Hidden &&
        deadline_us == 0,
        "system ownership must bypass animation and hide a visible corner immediately");

    state = HomeCornerTransitionState::Showing;
    deadline_us = ordinary_hide_us + 2000000;
    Check(
        RequestHomeCornerTransition(
            false,
            true,
            ordinary_hide_us + 1200000,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::HideImmediately &&
        state == HomeCornerTransitionState::Hidden &&
        deadline_us == 0,
        "immediate suppression must cancel an in-flight show");

    state = HomeCornerTransitionState::Hiding;
    deadline_us = ordinary_hide_us + 2000000;
    Check(
        RequestHomeCornerTransition(
            false,
            true,
            ordinary_hide_us + 1300000,
            state,
            deadline_us) ==
            HomeCornerTransitionAction::HideImmediately &&
        state == HomeCornerTransitionState::Hidden &&
        deadline_us == 0,
        "teardown must finish an in-flight hide without leaving stale state");

    return failures == 0 ? 0 : 1;
}

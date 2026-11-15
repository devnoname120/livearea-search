#include <livearea_search/home_corner_transition.h>

namespace livearea_search {

HomeCornerTransitionAction RequestHomeCornerTransition(
    bool visible,
    bool immediate,
    std::uint64_t now_us,
    HomeCornerTransitionState& state,
    std::uint64_t& deadline_us) noexcept {
    if (visible) {
        if (state == HomeCornerTransitionState::Showing ||
            state == HomeCornerTransitionState::Visible) {
            return HomeCornerTransitionAction::None;
        }
        state = HomeCornerTransitionState::Showing;
        deadline_us = now_us + kHomeCornerShowDurationUs;
        return HomeCornerTransitionAction::ShowNative;
    }

    if (state == HomeCornerTransitionState::Hidden) {
        deadline_us = 0;
        return HomeCornerTransitionAction::None;
    }
    if (immediate) {
        state = HomeCornerTransitionState::Hidden;
        deadline_us = 0;
        return HomeCornerTransitionAction::HideImmediately;
    }
    if (state == HomeCornerTransitionState::Hiding) {
        return HomeCornerTransitionAction::None;
    }
    state = HomeCornerTransitionState::Hiding;
    deadline_us = now_us + kHomeCornerHideDurationUs;
    return HomeCornerTransitionAction::HideNative;
}

HomeCornerTransitionCompletion AdvanceHomeCornerTransition(
    std::uint64_t now_us,
    HomeCornerTransitionState& state,
    std::uint64_t& deadline_us) noexcept {
    if (deadline_us == 0 || now_us < deadline_us) {
        return HomeCornerTransitionCompletion::None;
    }
    deadline_us = 0;
    if (state == HomeCornerTransitionState::Showing) {
        state = HomeCornerTransitionState::Visible;
        return HomeCornerTransitionCompletion::Show;
    }
    if (state == HomeCornerTransitionState::Hiding) {
        state = HomeCornerTransitionState::Hidden;
        return HomeCornerTransitionCompletion::Hide;
    }
    return HomeCornerTransitionCompletion::None;
}

} // namespace livearea_search

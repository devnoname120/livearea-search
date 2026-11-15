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

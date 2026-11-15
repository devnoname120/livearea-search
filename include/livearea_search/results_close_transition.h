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

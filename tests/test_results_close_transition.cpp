#include <livearea_search/results_close_transition.h>

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
    using livearea_search::AdvanceResultsCloseTransition;
    using livearea_search::QueueResultsClose;
    using livearea_search::ResultsCloseAction;
    using livearea_search::ResultsCloseMode;
    using livearea_search::ResultsCloseState;
    using livearea_search::kResultsBackHideDurationUs;

    ResultsCloseState state = ResultsCloseState::Idle;
    std::uint64_t deadline_us = 0;
    const std::uint64_t start_us = 1000000;

    QueueResultsClose(
        ResultsCloseMode::AnimatedBackCorner,
        state,
        deadline_us);
    Check(
        state == ResultsCloseState::PendingAnimated && deadline_us == 0,
        "ordinary close queues without running inside the callback");
    Check(
        AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
                ResultsCloseAction::StartBackCornerHide &&
            state == ResultsCloseState::AnimatingBackCorner &&
            deadline_us == start_us + kResultsBackHideDurationUs,
        "the first main-thread tick starts one native hide interval");

    QueueResultsClose(
        ResultsCloseMode::AnimatedBackCorner,
        state,
        deadline_us);
    Check(
        deadline_us == start_us + kResultsBackHideDurationUs,
        "repeated ordinary requests do not restart the deadline");
    Check(
        AdvanceResultsCloseTransition(
            start_us + kResultsBackHideDurationUs - 1,
            state,
            deadline_us) == ResultsCloseAction::None,
        "teardown waits for the complete firmware hide duration");
    Check(
        AdvanceResultsCloseTransition(
            start_us + kResultsBackHideDurationUs,
            state,
            deadline_us) == ResultsCloseAction::Close &&
            state == ResultsCloseState::Idle && deadline_us == 0,
        "teardown runs at the hide deadline and resets state");

    QueueResultsClose(ResultsCloseMode::Immediate, state, deadline_us);
    Check(
        state == ResultsCloseState::PendingImmediate,
        "safety close is queued for callback-safe teardown");
    Check(
        AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
                ResultsCloseAction::Close &&
            state == ResultsCloseState::Idle,
        "safety close executes on the next main-thread tick");

    QueueResultsClose(
        ResultsCloseMode::AnimatedBackCorner,
        state,
        deadline_us);
    Check(
        AdvanceResultsCloseTransition(start_us, state, deadline_us) ==
            ResultsCloseAction::StartBackCornerHide,
        "a second ordinary close can begin after reset");
    QueueResultsClose(ResultsCloseMode::Immediate, state, deadline_us);
    Check(
        state == ResultsCloseState::PendingImmediate && deadline_us == 0,
        "safety ownership preempts an active animation");
    Check(
        AdvanceResultsCloseTransition(start_us + 1, state, deadline_us) ==
            ResultsCloseAction::Close,
        "preempted animation closes on the next tick");

    return failures == 0 ? 0 : 1;
}

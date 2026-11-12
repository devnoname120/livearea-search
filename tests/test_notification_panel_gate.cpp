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
    using livearea_search::AdvanceNotificationPanelSuppression;
    using livearea_search::ClassifyIndicatorPopupOwnership;
    using livearea_search::ClassifyIndicatorPopupVisibility;
    using livearea_search::IndicatorPopupVisibility;
    using livearea_search::NotificationPanelSuppression;
    using livearea_search::ObserveNotificationCornerPress;
    using livearea_search::kNotificationPanelOpeningGraceUs;

    Check(
        ClassifyIndicatorPopupVisibility(true, -1, 1) ==
            IndicatorPopupVisibility::Visible,
        "a populated opened-page vector must remain authoritative when PAF returns its -1 sentinel");
    Check(
        ClassifyIndicatorPopupVisibility(true, -1, 0) ==
            IndicatorPopupVisibility::Hidden,
        "an empty opened-page vector with PAF's -1 sentinel must mean no popup is open");
    Check(
        ClassifyIndicatorPopupVisibility(true, -2, 0) ==
            IndicatorPopupVisibility::Unknown,
        "an unproven negative return with no pages must fail closed");
    Check(
        ClassifyIndicatorPopupVisibility(true, -2, 1) ==
            IndicatorPopupVisibility::Visible,
        "a nonempty vector must fail closed as visible even when the return code is unknown");
    Check(
        ClassifyIndicatorPopupVisibility(false, -1, 0) ==
            IndicatorPopupVisibility::Unknown,
        "an unavailable indicator plugin must not be classified as hidden");
    Check(
        ClassifyIndicatorPopupOwnership(
            true,
            true,
            IndicatorPopupVisibility::Hidden) ==
            IndicatorPopupVisibility::Visible,
        "an instantiated notification corner must prove panel ownership before opened-page registration");
    Check(
        ClassifyIndicatorPopupOwnership(
            true,
            false,
            IndicatorPopupVisibility::Visible) ==
            IndicatorPopupVisibility::Visible,
        "a registered opened page must retain panel ownership after the corner lookup clears");
    Check(
        ClassifyIndicatorPopupOwnership(
            true,
            false,
            IndicatorPopupVisibility::Hidden) ==
            IndicatorPopupVisibility::Hidden,
        "an available static root with neither corner nor opened page must be hidden");
    Check(
        ClassifyIndicatorPopupOwnership(
            false,
            false,
            IndicatorPopupVisibility::Hidden) ==
            IndicatorPopupVisibility::Unknown,
        "a missing static indicator root must fail closed even when page enumeration is empty");
    Check(
        ClassifyIndicatorPopupOwnership(
            false,
            false,
            IndicatorPopupVisibility::Visible) ==
            IndicatorPopupVisibility::Visible,
        "a known opened page must remain authoritative when the static root cannot be read");

    NotificationPanelSuppression state = NotificationPanelSuppression::Inactive;
    std::uint64_t opening_deadline = 0;
    Check(
        ObserveNotificationCornerPress(
            false,
            true,
            920,
            25,
            1000000U,
            state,
            opening_deadline),
        "a top-right press must suppress Home Search before Sony opens the panel");
    Check(
        state == NotificationPanelSuppression::Triggered,
        "a notification-corner press must arm the opening grace state");
    Check(
        opening_deadline == 1000000U + kNotificationPanelOpeningGraceUs,
        "the opening grace deadline must be based on monotonic system time");
    Check(
        AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Hidden,
            1000000U,
            opening_deadline,
            state),
        "the same-frame hidden query must not undo pre-dispatch suppression");
    Check(
        state == NotificationPanelSuppression::Triggered,
        "a pre-open hidden query must retain the opening phase");
    Check(
        AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Hidden,
            opening_deadline - 1U,
            opening_deadline,
            state),
        "repeated pre-open hidden queries must remain suppressed through the deadline");
    Check(
        !AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Hidden,
            opening_deadline,
            opening_deadline,
            state),
        "a failed panel open must release suppression at the bounded deadline");
    Check(
        state == NotificationPanelSuppression::Inactive,
        "a failed panel open must return the gate to inactive");

    Check(
        ObserveNotificationCornerPress(
            false,
            true,
            920,
            25,
            2000000U,
            state,
            opening_deadline),
        "a later notification press must rearm suppression");
    Check(
        AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Visible,
            2100000U,
            opening_deadline,
            state),
        "a visible indicator popup must keep Home Search suppressed");
    Check(
        state == NotificationPanelSuppression::WaitingForClose,
        "observing the popup must transition to close tracking");
    Check(
        AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Unknown,
            2200000U,
            opening_deadline,
            state),
        "an unreadable indicator state must fail closed while suppression is active");
    Check(
        !AdvanceNotificationPanelSuppression(
            IndicatorPopupVisibility::Hidden,
            2300000U,
            opening_deadline,
            state),
        "the first confirmed post-panel frame must release suppression");
    Check(
        state == NotificationPanelSuppression::Inactive,
        "confirmed panel closure must return the gate to its inactive state");

    state = NotificationPanelSuppression::Inactive;
    opening_deadline = 0;
    Check(
        !ObserveNotificationCornerPress(
            false, false, 920, 25, 3000000U, state, opening_deadline),
        "a touch release must not arm notification suppression");
    Check(
        !ObserveNotificationCornerPress(
            false, true, 871, 25, 3000000U, state, opening_deadline),
        "a press left of the notification bubble must not suppress Search");
    Check(
        !ObserveNotificationCornerPress(
            false, true, 920, 80, 3000000U, state, opening_deadline),
        "a press below the notification bubble must not suppress Search");
    Check(
        !ObserveNotificationCornerPress(
            true, true, 920, 25, 3000000U, state, opening_deadline),
        "an already-open Search page must not reuse the Home-button suppression path");
    Check(
        ObserveNotificationCornerPress(
            false, true, 872, 79, 3000000U, state, opening_deadline),
        "the inclusive top-right bubble edge must be accepted");
    const std::uint64_t first_deadline = opening_deadline;
    Check(
        !ObserveNotificationCornerPress(
            false, true, 920, 25, 3000100U, state, opening_deadline),
        "a close press while suppression is active must not rearm the opening grace");
    Check(
        opening_deadline == first_deadline,
        "an active suppression deadline must remain tied to the opening press");

    return failures == 0 ? 0 : 1;
}

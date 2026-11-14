#include <livearea_search/overlay_visibility.h>

namespace livearea_search {

IndicatorPopupVisibility ClassifyIndicatorPopupVisibility(
    bool plugin_available,
    std::int32_t query_result,
    std::size_t opened_page_count) noexcept {
    if (!plugin_available) {
        return IndicatorPopupVisibility::Unknown;
    }
    if (opened_page_count != 0U) {
        return IndicatorPopupVisibility::Visible;
    }
    if (query_result >= 0 || query_result == -1) {
        return IndicatorPopupVisibility::Hidden;
    }
    return IndicatorPopupVisibility::Unknown;
}

IndicatorPopupVisibility ClassifyIndicatorPopupOwnership(
    bool static_root_available,
    bool notification_corner_exists,
    IndicatorPopupVisibility opened_pages) noexcept {
    if (notification_corner_exists ||
        opened_pages == IndicatorPopupVisibility::Visible) {
        return IndicatorPopupVisibility::Visible;
    }
    if (!static_root_available ||
        opened_pages == IndicatorPopupVisibility::Unknown) {
        return IndicatorPopupVisibility::Unknown;
    }
    return IndicatorPopupVisibility::Hidden;
}

HomeVisibilityAction ComputeHomeVisibilityAction(
    bool currently_visible,
    bool topmenu_visible,
    bool results_open,
    bool immediate_hide) noexcept {
    const bool should_show = topmenu_visible && !results_open;
    if (should_show == currently_visible) {
        return HomeVisibilityAction::None;
    }
    if (should_show) {
        return HomeVisibilityAction::Show;
    }
    return immediate_hide || results_open ?
        HomeVisibilityAction::HideImmediately :
        HomeVisibilityAction::HideWithFade;
}

bool ShouldCloseResultsPage(
    bool shell_allows_overlay,
    bool topmenu_visible,
    bool results_open,
    bool ime_transition_active) noexcept {
    if (!results_open) {
        return false;
    }
    if (ime_transition_active) {
        return false;
    }
    if (!shell_allows_overlay) {
        return true;
    }
    return !topmenu_visible;
}

bool IsBubbleHomeSurface(
    std::uint32_t visible_page_hash,
    float,
    float,
    float target_x,
    float target_y) noexcept {
    const float tolerance = 0.5f;
    return visible_page_hash == kTopMenuHomePageHash &&
           target_x >= -tolerance && target_x <= tolerance &&
           target_y >= -tolerance && target_y <= tolerance;
}

bool IsHomeEditOverlayWidget(
    bool visible,
    bool widget_base,
    float position_x,
    float position_y,
    float width,
    float height) noexcept {
    return visible &&
        widget_base &&
        position_x >= -32.0f && position_x <= 32.0f &&
        position_y >= -32.0f && position_y <= 32.0f &&
        width >= 960.0f &&
        height >= 544.0f;
}

bool IsHomeEditScrollWidget(
    bool visible,
    bool scroll_view,
    float position_x,
    float position_y,
    float width,
    float height) noexcept {
    return visible &&
        scroll_view &&
        position_x >= -1.0f && position_x <= 1.0f &&
        position_y >= -1.0f && position_y <= 1.0f &&
        width >= 959.0f && width <= 961.0f &&
        height >= 543.0f && height <= 545.0f;
}

bool IsHomeBackCornerWidget(
    bool visible,
    bool corner_button,
    float position_x,
    float position_y,
    float width,
    float height) noexcept {
    return visible &&
        corner_button &&
        position_x <= -400.0f &&
        position_y <= -220.0f &&
        width >= 64.0f && width <= 128.0f &&
        height >= 64.0f && height <= 128.0f;
}

bool ShouldExposeHomeSearch(
    bool shell_allows_overlay,
    bool bubble_home_visible,
    bool event_notification_visible,
    bool livearea_visible,
    bool external_ime_visible,
    bool home_edit_mode_visible) noexcept {
    return shell_allows_overlay &&
        bubble_home_visible &&
        !event_notification_visible &&
        !livearea_visible &&
        !external_ime_visible &&
        !home_edit_mode_visible;
}

bool ObserveNotificationCornerPress(
    bool results_open,
    bool touch_pressed,
    std::int32_t x,
    std::int32_t y,
    std::uint64_t current_time_us,
    NotificationPanelSuppression& suppression,
    std::uint64_t& opening_deadline_us) noexcept {
    const bool inside_notification_corner =
        x >= 872 && x < 960 && y >= 0 && y < 80;
    if (results_open || !touch_pressed || !inside_notification_corner ||
        suppression != NotificationPanelSuppression::Inactive) {
        return false;
    }
    suppression = NotificationPanelSuppression::Triggered;
    opening_deadline_us = current_time_us + kNotificationPanelOpeningGraceUs;
    if (opening_deadline_us < current_time_us) {
        opening_deadline_us = UINT64_MAX;
    }
    return true;
}

bool AdvanceNotificationPanelSuppression(
    IndicatorPopupVisibility popup_visibility,
    std::uint64_t current_time_us,
    std::uint64_t opening_deadline_us,
    NotificationPanelSuppression& suppression) noexcept {
    if (suppression == NotificationPanelSuppression::Inactive) {
        return false;
    }
    if (suppression == NotificationPanelSuppression::Triggered) {
        if (popup_visibility == IndicatorPopupVisibility::Visible) {
            suppression = NotificationPanelSuppression::WaitingForClose;
            return true;
        }
        if (popup_visibility == IndicatorPopupVisibility::Unknown ||
            current_time_us < opening_deadline_us) {
            return true;
        }
        suppression = NotificationPanelSuppression::Inactive;
        return false;
    }
    if (popup_visibility == IndicatorPopupVisibility::Unknown) {
        return true;
    }
    if (popup_visibility == IndicatorPopupVisibility::Visible) {
        return true;
    }
    suppression = NotificationPanelSuppression::Inactive;
    return false;
}

bool ObserveHomePadPress(
    bool results_open,
    std::uint32_t ps_button_mask,
    std::uint32_t& forwarded_buttons,
    bool& ps_owned) noexcept {
    const bool pressed = (forwarded_buttons & ps_button_mask) != 0U;
    if (ps_owned) {
        if (pressed) {
            forwarded_buttons &= ~ps_button_mask;
        }
        if (!pressed) {
            ps_owned = false;
        }
        return false;
    }
    if (!results_open || !pressed) {
        return false;
    }
    ps_owned = true;
    forwarded_buttons &= ~ps_button_mask;
    return true;
}

HomePadCloseAction ComputeHomePadCloseAction(
    bool close_pending,
    bool ps_owned) noexcept {
    if (!close_pending) {
        return HomePadCloseAction::None;
    }
    return ps_owned ?
        HomePadCloseAction::WaitForRelease :
        HomePadCloseAction::ReleaseLockAndClose;
}

bool ShouldAdoptExternalImeEdit(
    bool results_open,
    bool transition_active,
    bool is_editing) noexcept {
    return results_open && !transition_active && is_editing;
}

ImeFrameAction ComputeImeFrameAction(
    bool transition_active,
    bool editing_seen,
    bool is_editing,
    bool show_results_pending,
    bool close_results_pending,
    bool shell_ready) noexcept {
    if (!transition_active) {
        return ImeFrameAction::None;
    }
    if (is_editing) {
        return editing_seen ? ImeFrameAction::None : ImeFrameAction::MarkEditing;
    }
    if (!editing_seen) {
        return ImeFrameAction::None;
    }
    if (close_results_pending) {
        return ImeFrameAction::CloseResults;
    }
    if (show_results_pending) {
        return shell_ready ? ImeFrameAction::ShowResults : ImeFrameAction::None;
    }
    return editing_seen ? ImeFrameAction::CloseResults : ImeFrameAction::None;
}

DeferredResultCloseAction AdvanceDeferredResultClose(
    bool close_requested,
    std::uint32_t& frames_remaining) noexcept {
    if (!close_requested) {
        return DeferredResultCloseAction::None;
    }
    if (frames_remaining != 0) {
        --frames_remaining;
        return DeferredResultCloseAction::Wait;
    }
    return DeferredResultCloseAction::Close;
}

ResultsBackAction ComputeResultsBackAction(
    bool results_open,
    bool ime_editing,
    bool ime_transition_active) noexcept {
    if (!results_open) {
        return ResultsBackAction::None;
    }
    if (ime_editing) {
        return ResultsBackAction::EndImeEdit;
    }
    if (ime_transition_active) {
        return ResultsBackAction::None;
    }
    return ResultsBackAction::CloseResults;
}

} // namespace livearea_search

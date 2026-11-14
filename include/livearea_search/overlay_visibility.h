#pragma once

#include <cstddef>
#include <cstdint>

namespace livearea_search {

const std::uint32_t kTopMenuHomePageHash = 0x5E9535B3U;
const std::uint32_t kTopMenuHomeScrollHash = 0x9CC405A3U;
const std::uint32_t kIndicatorPageHash = 0xFFD7E3F0U;
const std::uint32_t kIndicatorNotificationCornerHash = 0x8BF9D3C5U;
const std::uint64_t kNotificationPanelOpeningGraceUs = 500000ULL;

enum class HomeVisibilityAction {
    None,
    Show,
    HideWithFade,
    HideImmediately,
};

enum class ImeFrameAction {
    None,
    MarkEditing,
    ShowResults,
    CloseResults,
};

enum class DeferredResultCloseAction {
    None,
    Wait,
    Close,
};

enum class ResultsBackAction {
    None,
    EndImeEdit,
    CloseResults,
};

enum class HomePadCloseAction {
    None,
    WaitForRelease,
    ReleaseLockAndClose,
};

enum class NotificationPanelSuppression {
    Inactive,
    Triggered,
    WaitingForClose,
};

enum class IndicatorPopupVisibility {
    Unknown,
    Hidden,
    Visible,
};

IndicatorPopupVisibility ClassifyIndicatorPopupVisibility(
    bool plugin_available,
    std::int32_t query_result,
    std::size_t opened_page_count) noexcept;
IndicatorPopupVisibility ClassifyIndicatorPopupOwnership(
    bool static_root_available,
    bool notification_corner_exists,
    IndicatorPopupVisibility opened_pages) noexcept;

HomeVisibilityAction ComputeHomeVisibilityAction(
    bool currently_visible,
    bool topmenu_visible,
    bool results_open,
    bool immediate_hide) noexcept;
bool ShouldCloseResultsPage(
    bool shell_allows_overlay,
    bool topmenu_visible,
    bool results_open,
    bool ime_transition_active) noexcept;
bool IsBubbleHomeSurface(
    std::uint32_t visible_page_hash,
    float focus_x,
    float focus_y,
    float target_x,
    float target_y) noexcept;
bool IsHomeEditOverlayWidget(
    bool visible,
    bool widget_base,
    float position_x,
    float position_y,
    float width,
    float height) noexcept;
bool IsHomeEditScrollWidget(
    bool visible,
    bool scroll_view,
    float position_x,
    float position_y,
    float width,
    float height) noexcept;
bool IsHomeBackCornerWidget(
    bool visible,
    bool corner_button,
    float position_x,
    float position_y,
    float width,
    float height) noexcept;
bool ShouldExposeHomeSearch(
    bool shell_allows_overlay,
    bool bubble_home_visible,
    bool event_notification_visible,
    bool livearea_visible,
    bool external_ime_visible,
    bool home_edit_mode_visible) noexcept;
bool ObserveNotificationCornerPress(
    bool results_open,
    bool touch_pressed,
    std::int32_t x,
    std::int32_t y,
    std::uint64_t current_time_us,
    NotificationPanelSuppression& suppression,
    std::uint64_t& opening_deadline_us) noexcept;
bool AdvanceNotificationPanelSuppression(
    IndicatorPopupVisibility popup_visibility,
    std::uint64_t current_time_us,
    std::uint64_t opening_deadline_us,
    NotificationPanelSuppression& suppression) noexcept;
bool ObserveHomePadPress(
    bool results_open,
    std::uint32_t ps_button_mask,
    std::uint32_t& forwarded_buttons,
    bool& ps_owned) noexcept;
bool ObserveStartButtonPress(
    bool home_search_visible,
    std::uint32_t start_button_mask,
    std::uint32_t& forwarded_buttons,
    bool& start_owned) noexcept;
HomePadCloseAction ComputeHomePadCloseAction(
    bool close_pending,
    bool ps_owned) noexcept;
bool ShouldAdoptExternalImeEdit(
    bool results_open,
    bool transition_active,
    bool is_editing) noexcept;
ImeFrameAction ComputeImeFrameAction(
    bool transition_active,
    bool editing_seen,
    bool is_editing,
    bool show_results_pending,
    bool close_results_pending,
    bool shell_ready) noexcept;
DeferredResultCloseAction AdvanceDeferredResultClose(
    bool close_requested,
    std::uint32_t& frames_remaining) noexcept;
ResultsBackAction ComputeResultsBackAction(
    bool results_open,
    bool ime_editing,
    bool ime_transition_active) noexcept;

} // namespace livearea_search

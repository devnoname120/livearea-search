#pragma once

#include <cstdint>

namespace livearea_search {

const std::uint32_t kTopMenuHomePageHash = 0x5E9535B3U;
const std::uint32_t kTopMenuHomeScrollHash = 0x9CC405A3U;

enum class HomeVisibilityAction {
    None,
    Show,
    Hide,
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

HomeVisibilityAction ComputeHomeVisibilityAction(
    bool currently_visible,
    bool topmenu_visible,
    bool results_open) noexcept;
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

} // namespace livearea_search

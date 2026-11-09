#pragma once

namespace livearea_search {

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

HomeVisibilityAction ComputeHomeVisibilityAction(
    bool currently_visible,
    bool topmenu_visible,
    bool results_open) noexcept;
bool ShouldCloseResultsPage(
    bool shell_allows_overlay,
    bool topmenu_visible,
    bool results_open,
    bool ime_transition_active) noexcept;
ImeFrameAction ComputeImeFrameAction(
    bool transition_active,
    bool editing_seen,
    bool is_editing,
    bool show_results_pending,
    bool close_results_pending,
    bool shell_ready) noexcept;

} // namespace livearea_search

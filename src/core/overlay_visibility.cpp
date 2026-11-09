#include <livearea_search/overlay_visibility.h>

namespace livearea_search {

HomeVisibilityAction ComputeHomeVisibilityAction(
    bool currently_visible,
    bool topmenu_visible,
    bool results_open) noexcept {
    const bool should_show = topmenu_visible && !results_open;
    if (should_show == currently_visible) {
        return HomeVisibilityAction::None;
    }
    return should_show ? HomeVisibilityAction::Show : HomeVisibilityAction::Hide;
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
    if (close_results_pending) {
        return ImeFrameAction::CloseResults;
    }
    if (show_results_pending) {
        return shell_ready ? ImeFrameAction::ShowResults : ImeFrameAction::None;
    }
    return editing_seen ? ImeFrameAction::CloseResults : ImeFrameAction::None;
}

} // namespace livearea_search

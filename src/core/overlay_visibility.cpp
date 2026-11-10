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

bool IsBubbleHomeSurface(
    std::uint32_t visible_page_hash,
    float focus_x,
    float focus_y,
    float target_x,
    float target_y) noexcept {
    const float tolerance = 0.5f;
    return visible_page_hash == kTopMenuHomePageHash &&
           focus_x >= -tolerance && focus_x <= tolerance &&
           focus_y >= -tolerance && focus_y <= tolerance &&
           target_x >= -tolerance && target_x <= tolerance &&
           target_y >= -tolerance && target_y <= tolerance;
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

} // namespace livearea_search

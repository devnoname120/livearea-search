#include <livearea_search/module_readiness.h>

#include <limits>

namespace livearea_search {

bool IsModuleLookupReady(int result, std::int32_t module_id) noexcept {
    return result >= 0 && module_id >= 0;
}

PreparedDataLoadAction ClassifyPreparedDataLoadResult(int result) noexcept {
    if (result >= 0) {
        return PreparedDataLoadAction::Publish;
    }
    if (static_cast<std::uint32_t>(result) == 0x80101003U) {
        return PreparedDataLoadAction::WaitForDatabase;
    }
    return PreparedDataLoadAction::Fail;
}

std::uint32_t AdvanceShellReadyFrames(
    std::uint32_t current_frames,
    bool shell_ready) noexcept {
    if (!shell_ready) {
        return 0;
    }
    if (current_frames == std::numeric_limits<std::uint32_t>::max()) {
        return current_frames;
    }
    return current_frames + 1;
}

bool HasStableShellReadiness(
    std::uint32_t ready_frames,
    std::uint32_t required_frames) noexcept {
    return required_frames != 0 && ready_frames >= required_frames;
}

bool IsShellLoadingSceneDismissed(
    bool environment_available,
    std::int32_t scene_count) noexcept {
    return environment_available &&
           (scene_count == 0 ||
            scene_count == kShellLoadingSceneMissingError);
}

bool CanLoadOverlay(
    bool running,
    bool active,
    bool load_in_progress,
    bool topmenu_available) noexcept {
    return running && !active && !load_in_progress && topmenu_available;
}

} // namespace livearea_search

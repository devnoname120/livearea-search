#include <livearea_search/module_readiness.h>

#include <limits>

namespace livearea_search {

bool IsModuleLookupReady(int result, std::int32_t module_id) noexcept {
    return result >= 0 && module_id >= 0;
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

bool CanLoadOverlay(
    bool running,
    bool active,
    bool load_in_progress,
    bool topmenu_available) noexcept {
    return running && !active && !load_in_progress && topmenu_available;
}

} // namespace livearea_search

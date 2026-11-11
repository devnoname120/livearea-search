#pragma once

#include <cstdint>

namespace livearea_search {

enum class PreparedDataLoadAction {
    Publish,
    WaitForDatabase,
    Fail,
};

const int kShellLoadingSceneEntry = 25;
const std::int32_t kShellLoadingSceneMissingError = -2135998193;

bool IsModuleLookupReady(int result, std::int32_t module_id) noexcept;
PreparedDataLoadAction ClassifyPreparedDataLoadResult(int result) noexcept;
std::uint32_t AdvanceShellReadyFrames(
    std::uint32_t current_frames,
    bool shell_ready) noexcept;
bool HasStableShellReadiness(
    std::uint32_t ready_frames,
    std::uint32_t required_frames) noexcept;
bool IsShellLoadingSceneDismissed(
    bool environment_available,
    std::int32_t scene_count) noexcept;
bool CanLoadOverlay(
    bool running,
    bool active,
    bool load_in_progress,
    bool shell_stable) noexcept;

} // namespace livearea_search

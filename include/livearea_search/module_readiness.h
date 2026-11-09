#pragma once

#include <cstdint>

namespace livearea_search {

bool IsModuleLookupReady(int result, std::int32_t module_id) noexcept;
std::uint32_t AdvanceShellReadyFrames(
    std::uint32_t current_frames,
    bool shell_ready) noexcept;
bool HasStableShellReadiness(
    std::uint32_t ready_frames,
    std::uint32_t required_frames) noexcept;
bool CanLoadOverlay(
    bool running,
    bool active,
    bool load_in_progress,
    bool shell_stable) noexcept;

} // namespace livearea_search

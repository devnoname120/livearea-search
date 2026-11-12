#pragma once

#include <cstddef>
#include <cstdint>

namespace livearea_search {

struct AppMgrAppStateLayout {
    std::uint32_t system_event_num;
    std::uint32_t app_event_num;
    std::uint32_t is_system_ui_overlaid;
    std::uint8_t reserved[0x80U - 3U * sizeof(std::uint32_t)];
};

static_assert(sizeof(AppMgrAppStateLayout) == 0x80,
              "SceAppMgr app-state size mismatch");
static_assert(offsetof(AppMgrAppStateLayout, is_system_ui_overlaid) == 0x08,
              "SceAppMgr system-UI overlay offset mismatch");

inline bool ShouldAllowShellOverlay(
    bool foreground_clear,
    bool system_ui_overlaid,
    bool explicitly_suppressed) noexcept {
    return foreground_clear && !system_ui_overlaid && !explicitly_suppressed;
}

} // namespace livearea_search

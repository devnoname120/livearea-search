#pragma once

#include <cstdint>

namespace paf {
class Plugin;
}

namespace livearea_search {

void BroadcastWidgetEvent(
    paf::Plugin* plugin,
    std::uint32_t type,
    std::int32_t value0 = 0,
    std::int32_t value1 = 0);

} // namespace livearea_search

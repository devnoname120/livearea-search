#pragma once

#include <cstddef>

namespace livearea_search {

constexpr std::size_t kConfigEntryNotFound = static_cast<std::size_t>(-1);

std::size_t FindConfigSelfDisableOffset(
    const char* contents,
    std::size_t size,
    std::size_t search_from = 0);

std::size_t CountConfigSelfDisableEntries(
    const char* contents,
    std::size_t size);

} // namespace livearea_search

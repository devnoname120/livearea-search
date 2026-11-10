#pragma once

#include <cstddef>
#include <cstdint>

namespace livearea_search {

const std::size_t kNoIconCacheEvictionCandidate = static_cast<std::size_t>(-1);

struct IconCacheEvictionEntry {
    bool resident;
    bool loading;
    bool exclusively_owned;
    std::size_t pin_count;
    std::uint64_t last_used;
};

std::size_t SelectIconCacheEvictionCandidate(
    const IconCacheEvictionEntry* entries,
    std::size_t count,
    std::size_t excluded_index = kNoIconCacheEvictionCandidate) noexcept;

} // namespace livearea_search

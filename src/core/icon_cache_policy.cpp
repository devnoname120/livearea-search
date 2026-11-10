#include <livearea_search/icon_cache_policy.h>

namespace livearea_search {

std::size_t SelectIconCacheEvictionCandidate(
    const IconCacheEvictionEntry* entries,
    std::size_t count,
    std::size_t excluded_index) noexcept {
    if (entries == nullptr) {
        return kNoIconCacheEvictionCandidate;
    }

    std::size_t candidate = kNoIconCacheEvictionCandidate;
    for (std::size_t index = 0; index < count; ++index) {
        const IconCacheEvictionEntry& entry = entries[index];
        if (index == excluded_index || !entry.resident || entry.loading ||
            !entry.exclusively_owned || entry.pin_count != 0) {
            continue;
        }
        if (candidate == kNoIconCacheEvictionCandidate ||
            entry.last_used < entries[candidate].last_used) {
            candidate = index;
        }
    }
    return candidate;
}

} // namespace livearea_search

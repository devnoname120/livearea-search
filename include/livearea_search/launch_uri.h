#pragma once

#include <cstddef>

#include <livearea_search/types.h>

namespace livearea_search {

const std::size_t kMaxTitleIdLength = 31;

bool IsValidTitleId(const String& title_id);
bool BuildOpenLiveAreaUri(const String& title_id, String& uri);

} // namespace livearea_search

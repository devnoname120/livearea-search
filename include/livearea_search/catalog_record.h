#pragma once

#include <cstdint>

#include <livearea_search/app_index.h>

namespace livearea_search {

struct CatalogMetadataView {
    const char* title_id;
    const char* title;
    const char* short_title;
    const char* app_name;
    const char* category;
    const char* icon_path;
    bool has_home_bubble;
    std::int64_t bootable;
};

bool BuildCatalogRecord(const CatalogMetadataView& metadata, AppRecord& record);

} // namespace livearea_search

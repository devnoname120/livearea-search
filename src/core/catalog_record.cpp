#include <livearea_search/catalog_record.h>
#include <livearea_search/launch_uri.h>

namespace livearea_search {

namespace {

const char* NonNull(const char* value) {
    return value == NULL ? "" : value;
}

} // namespace

bool BuildCatalogRecord(const CatalogMetadataView& metadata, AppRecord& record) {
    const String title_id(NonNull(metadata.title_id));
    if (!metadata.has_home_bubble ||
        metadata.bootable == 0 ||
        !IsValidTitleId(title_id)) {
        return false;
    }

    String display_title(NonNull(metadata.title));
    if (NormalizeForSearch(display_title).empty()) {
        display_title = NonNull(metadata.short_title);
    }
    if (NormalizeForSearch(display_title).empty()) {
        display_title = NonNull(metadata.app_name);
    }
    if (NormalizeForSearch(display_title).empty()) {
        display_title = title_id;
    }

    record.title_id = title_id;
    record.display_title = display_title;
    record.app_name = NonNull(metadata.app_name);
    record.category = NonNull(metadata.category);
    record.icon_path = NonNull(metadata.icon_path);
    record.launchable = true;
    return true;
}

} // namespace livearea_search

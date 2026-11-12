#include "lsdb_index_source.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <taihen.h>

#include <livearea_search/catalog_record.h>

#include "paf_compat.h"
#include "startup_log.h"

namespace livearea_search {

namespace {

template <typename Function>
int ResolveFunction(std::uint32_t nid, Function& function) noexcept {
    std::uintptr_t address = 0;
    const int result = taiGetModuleExportFunc(
        lsdb::kModuleName,
        lsdb::kLibraryNid,
        nid,
        &address);
    if (result < 0) {
        return result;
    }
    if (address == 0) {
        return -1;
    }
    function = reinterpret_cast<Function>(address);
    return 0;
}

const char* NonNull(const char* value) {
    return value == nullptr ? "" : value;
}

String BuildIconPath(const char* title_id_value) {
    const String title_id(NonNull(title_id_value));
    String path;
    // LiveArea's ur0 copy is plaintext; retail ux0 application assets may be encrypted.
    path.reserve(24 + title_id.size());
    path.append("ur0:appmeta/");
    path.append(title_id);
    path.append("/icon0.png");
    return path;
}

void ReleaseStringArray(lsdb::StringArray& strings) {
    for (std::uint32_t index = 0; index < strings.count; ++index) {
        if (strings.data[index].data != NULL && strings.data[index].length != 0U) {
            sce_paf_free(strings.data[index].data);
        }
    }
    if (strings.data != NULL) {
        sce_paf_free(strings.data);
    }
    strings.data = NULL;
    strings.count = 0U;
    strings.capacity = 0U;
}

int QueryHomeBubbleTitleIds(
    lsdb::GetFilteredTitleIds get_filtered_title_ids,
    lsdb::AppDatabase& database,
    Vector<String>& title_ids) {
    const lsdb::AppInfoFilter filter{
        lsdb::kCategoryKey,
        {NULL, 0U, 0U},
        0U,
        lsdb::kFilterOperatorEqual,
        0U,
        NULL};
    lsdb::StringArray output{NULL, 0U, 0U};
    const int result = get_filtered_title_ids(
        &database,
        &filter,
        &output,
        lsdb::kIconRowFilterFlags);
    if (result >= 0) {
        title_ids.reserve(output.count);
        for (std::uint32_t index = 0; index < output.count; ++index) {
            const lsdb::String& title_id = output.data[index];
            if (title_id.data != NULL && title_id.length != 0U) {
                title_ids.push_back(String(title_id.data, title_id.length));
            }
        }
    }
    ReleaseStringArray(output);
    if (result < 0) {
        return result;
    }
    std::sort(title_ids.begin(), title_ids.end());
    title_ids.erase(std::unique(title_ids.begin(), title_ids.end()), title_ids.end());
    return 0;
}

bool ContainsTitleId(const Vector<String>& title_ids, const String& title_id) {
    return std::binary_search(title_ids.begin(), title_ids.end(), title_id);
}

} // namespace

int LsdbIndexSource::Resolve() noexcept {
    StartupLog("lsdb resolve begin");
    Api resolved{};
    int result = ResolveFunction(lsdb::kAppDatabaseInitNid, resolved.app_database_init);
    if (result < 0) {
        StartupLogResult("lsdb resolve app_database_init", result);
        return result;
    }
    result = ResolveFunction(lsdb::kAppDatabaseEndNid, resolved.app_database_end);
    if (result < 0) {
        StartupLogResult("lsdb resolve app_database_end", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetAllAppInfoNid, resolved.get_all_app_info);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_all_app_info", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetTitleIdNid, resolved.get_title_id);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_title_id", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetTitleNid, resolved.get_title);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_title", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetShortTitleNid, resolved.get_short_title);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_short_title", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetNameNid, resolved.get_name);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_name", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetCategoryNid, resolved.get_category);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_category", result);
        return result;
    }
    result = ResolveFunction(lsdb::kGetBootableNid, resolved.get_bootable);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_bootable", result);
        return result;
    }
    result = ResolveFunction(
        lsdb::kGetFilteredTitleIdsNid,
        resolved.get_filtered_title_ids);
    if (result < 0) {
        StartupLogResult("lsdb resolve get_filtered_title_ids", result);
        return result;
    }

    api_ = resolved;
    StartupLog("lsdb resolve complete");
    return 0;
}

bool LsdbIndexSource::resolved() const noexcept {
    return api_.app_database_init != nullptr &&
           api_.app_database_end != nullptr &&
           api_.get_all_app_info != nullptr &&
           api_.get_title_id != nullptr &&
           api_.get_title != nullptr &&
           api_.get_short_title != nullptr &&
           api_.get_name != nullptr &&
           api_.get_category != nullptr &&
           api_.get_bootable != nullptr &&
           api_.get_filtered_title_ids != nullptr;
}

int LsdbIndexSource::Load(AppIndex& index) const {
    StartupLog("lsdb load begin");
    if (!resolved()) {
        return -1;
    }

    lsdb::AppDatabase database{};
    // SceShell owns the process-shared AppDB connection; opening it again is invalid.
    if (api_.app_database_init(&database) == nullptr || database.token == nullptr) {
        StartupLog("lsdb app_database_init returned null");
        return -1;
    }

    Vector<CatalogCandidate> candidates;
    EnumerationContext context{&api_, &candidates};
    const int result = api_.get_all_app_info(&database, OnAppInfo, &context);
    StartupLogResult("lsdb get_all_app_info", result);
    StartupLogResult("lsdb enumerated record count", static_cast<int>(candidates.size()));
    Vector<String> home_bubble_title_ids;
    const int filter_result = result < 0 ? result :
        QueryHomeBubbleTitleIds(
            api_.get_filtered_title_ids,
            database,
            home_bubble_title_ids);
    StartupLogResult("lsdb get filtered title ids", filter_result);
    StartupLog("lsdb app_database_end begin");
    api_.app_database_end(&database);
    StartupLog("lsdb app_database_end returned");
    if (result < 0) {
        return result;
    }
    if (filter_result < 0) {
        return filter_result;
    }

    Vector<AppRecord> records;
    records.reserve(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const CatalogCandidate& candidate = candidates[index];
        AppRecord record;
        if (BuildCatalogRecord(CatalogMetadataView{
                candidate.title_id.c_str(),
                candidate.title.c_str(),
                candidate.short_title.c_str(),
                candidate.app_name.c_str(),
                candidate.category.c_str(),
                candidate.icon_path.c_str(),
                ContainsTitleId(home_bubble_title_ids, candidate.title_id),
                candidate.bootable},
                record)) {
            records.push_back(std::move(record));
        }
    }

    StartupLog("lsdb sort and deduplicate begin");
    const std::size_t record_count = records.size();
    SortAndDeduplicateAppRecords(records);
    StartupLogResult(
        "lsdb duplicate record count",
        static_cast<int>(record_count - records.size()));
    StartupLog("lsdb sort and deduplicate returned");
    StartupLog("lsdb index assign begin");
    index.Assign(records);
    StartupLog("lsdb index assign returned");
    StartupLogResult("lsdb accepted record count", static_cast<int>(index.apps().size()));
    return 0;
}

void LsdbIndexSource::OnAppInfo(const lsdb::AppInfo* app_info, void* user_data) {
    if (app_info == nullptr || user_data == nullptr) {
        return;
    }

    auto* context = static_cast<EnumerationContext*>(user_data);
    if (context->api == nullptr ||
        context->candidates == nullptr) {
        return;
    }

    const Api& api = *context->api;
    const char* title_id = NonNull(api.get_title_id(app_info));
    CatalogCandidate candidate;
    candidate.title_id = title_id;
    candidate.title = NonNull(api.get_title(app_info));
    candidate.short_title = NonNull(api.get_short_title(app_info));
    candidate.app_name = NonNull(api.get_name(app_info));
    candidate.category = NonNull(api.get_category(app_info));
    candidate.icon_path = BuildIconPath(title_id);
    candidate.bootable = api.get_bootable(app_info);
    context->candidates->push_back(std::move(candidate));
}

} // namespace livearea_search

#include "lsdb_index_source.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <taihen.h>

#include <livearea_search/catalog_record.h>

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
           api_.get_bootable != nullptr;
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

    Vector<AppRecord> records;
    EnumerationContext context{&api_, &records};
    const int result = api_.get_all_app_info(&database, OnAppInfo, &context);
    StartupLogResult("lsdb get_all_app_info", result);
    StartupLogResult("lsdb enumerated record count", static_cast<int>(records.size()));
    StartupLog("lsdb app_database_end begin");
    api_.app_database_end(&database);
    StartupLog("lsdb app_database_end returned");
    if (result < 0) {
        return result;
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
    if (context->api == nullptr || context->records == nullptr) {
        return;
    }

    const Api& api = *context->api;
    const char* title_id = NonNull(api.get_title_id(app_info));
    const String icon_path = BuildIconPath(title_id);
    AppRecord record;
    if (BuildCatalogRecord(CatalogMetadataView{
        title_id,
        NonNull(api.get_title(app_info)),
        NonNull(api.get_short_title(app_info)),
        NonNull(api.get_name(app_info)),
        NonNull(api.get_category(app_info)),
        icon_path.c_str(),
        api.get_bootable(app_info)}, record)) {
        context->records->push_back(record);
    }
}

} // namespace livearea_search

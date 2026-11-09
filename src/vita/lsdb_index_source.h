#pragma once

#include <cstdint>
#include <livearea_search/app_index.h>
#include <livearea_search/lsdb_abi.h>

namespace livearea_search {

class LsdbIndexSource {
public:
    int Resolve() noexcept;
    int Load(AppIndex& index) const;
    bool resolved() const noexcept;

private:
    struct Api {
        lsdb::AppDatabaseInit app_database_init{};
        lsdb::AppDatabaseEnd app_database_end{};
        lsdb::GetAllAppInfo get_all_app_info{};
        lsdb::GetString get_title_id{};
        lsdb::GetString get_title{};
        lsdb::GetString get_short_title{};
        lsdb::GetString get_name{};
        lsdb::GetString get_category{};
        lsdb::GetInt64 get_bootable{};
    };

    struct EnumerationContext {
        EnumerationContext(const Api* api_value, Vector<AppRecord>* records_value)
            : api(api_value), records(records_value) {}

        const Api* api;
        Vector<AppRecord>* records;
    };

    static void OnAppInfo(const lsdb::AppInfo* app_info, void* user_data);

    Api api_{};
};

} // namespace livearea_search

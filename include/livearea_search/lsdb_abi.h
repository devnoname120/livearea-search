#pragma once

#include <cstdint>

namespace livearea_search {
namespace lsdb {

static const char kModuleName[] = "SceLsdb";
static const std::uint32_t kLibraryNid = 0x6BC25E17U;

static const std::uint32_t kAppDatabaseInitNid = 0xD322A870U;
static const std::uint32_t kAppDatabaseEndNid = 0x56D8DCF0U;
static const std::uint32_t kGetAllAppInfoNid = 0xA6CEEAB3U;
static const std::uint32_t kGetTitleIdNid = 0x1F569CFCU;
static const std::uint32_t kGetTitleNid = 0x4141EBCDU;
static const std::uint32_t kGetShortTitleNid = 0x3B064DF5U;
static const std::uint32_t kGetNameNid = 0xD02A8B85U;
static const std::uint32_t kGetCategoryNid = 0x6469796FU;
static const std::uint32_t kGetBootableNid = 0x46BF2766U;

struct AppDatabase {
    void* token;
};

struct AppInfo {
    void* data;
    void* control;
};

using AppInfoCallback = void (*)(const AppInfo* app_info, void* user_data);
using AppDatabaseInit = AppDatabase* (*)(AppDatabase* database);
using AppDatabaseEnd = AppDatabase* (*)(AppDatabase* database);
using GetAllAppInfo = int (*)(
    AppDatabase* database,
    AppInfoCallback callback,
    void* user_data);
using GetString = const char* (*)(const AppInfo* app_info);
using GetInt64 = std::int64_t (*)(const AppInfo* app_info);

#if defined(__vita__) || defined(__psp2__)
static_assert(sizeof(AppDatabase) == 0x4, "SceLsdbAppDatabase ABI mismatch");
static_assert(sizeof(AppInfo) == 0x8, "SceLsdbAppInfo ABI mismatch");
#endif

} // namespace lsdb
} // namespace livearea_search

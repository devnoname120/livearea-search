if(NOT DEFINED LSDB_ABI OR
   NOT DEFINED CATALOG_HEADER OR
   NOT DEFINED LSDB_HEADER OR
   NOT DEFINED LSDB_SOURCE)
    message(FATAL_ERROR "All LSDB Home-bubble contract paths are required")
endif()

file(READ "${LSDB_ABI}" lsdb_abi)
file(READ "${CATALOG_HEADER}" catalog_header)
file(READ "${LSDB_HEADER}" lsdb_header)
file(READ "${LSDB_SOURCE}" lsdb_source)

foreach(required IN ITEMS
    "kGetFilteredTitleIdsNid = 0xDD7D68C0U"
    "kCategoryKey = 0x21CA76B1U"
    "kIconRowFilterFlags = 0U"
    "struct StringArray"
    "struct AppInfoFilter"
    "sizeof(AppDatabase) == 0x4"
    "sizeof(AppInfo) == 0x8"
    "sizeof(String) == 0xC"
    "sizeof(StringArray) == 0xC"
    "sizeof(ValueArray) == 0xC"
    "sizeof(AppInfoFilter) == 0x24"
    "using GetFilteredTitleIds = int (*)"
    "bool has_home_bubble;"
    "lsdb::GetFilteredTitleIds get_filtered_title_ids{};"
    "ResolveFunction("
    "lsdb::kGetFilteredTitleIdsNid,"
    "resolved.get_filtered_title_ids)"
    "QueryHomeBubbleTitleIds("
    "api_.get_filtered_title_ids,"
    "get_filtered_title_ids("
    "const lsdb::AppInfoFilter filter{"
    "{NULL, 0U, 0U}"
    "lsdb::kCategoryKey"
    "lsdb::kIconRowFilterFlags"
    "ReleaseStringArray(output);"
    "sce_paf_free(strings.data[index].data);"
    "sce_paf_free(strings.data);"
    "ContainsTitleId(home_bubble_title_ids, candidate.title_id)"
    "has_home_bubble")
    string(FIND "${lsdb_abi}${catalog_header}${lsdb_header}${lsdb_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing firmware-backed Home-bubble filtering: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "GetIconInfoByTitleId"
    "get_icon_info_by_title_id"
    "paf::common::SharedPtr<lsdb::IconInfo>"
    "NPXS"
    "PCSA"
    "PCSE")
    string(FIND "${lsdb_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Home-bubble eligibility must not use an ad-hoc blacklist: ${forbidden}")
    endif()
endforeach()

string(FIND "${lsdb_source}" "api_.get_all_app_info(" enumeration_index)
string(FIND "${lsdb_source}"
    "QueryHomeBubbleTitleIds(\n            api_.get_filtered_title_ids,"
    query_call_index)
if(enumeration_index EQUAL -1 OR query_call_index EQUAL -1 OR
   NOT enumeration_index LESS query_call_index)
    message(FATAL_ERROR "Home-bubble query must be a post-enumeration operation, never a callback query")
endif()

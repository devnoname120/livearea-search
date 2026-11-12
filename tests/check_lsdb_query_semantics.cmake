if(NOT DEFINED LSDB_ABI OR NOT DEFINED LSDB_SOURCE OR NOT DEFINED SEMANTICS_REPORT)
    message(FATAL_ERROR "LSDB_ABI, LSDB_SOURCE and SEMANTICS_REPORT are required")
endif()

file(READ "${LSDB_ABI}" abi)
file(READ "${LSDB_SOURCE}" source)
file(READ "${SEMANTICS_REPORT}" report)

foreach(required IN ITEMS
    "kGetFilteredTitleIdsNid = 0xDD7D68C0U;"
    "kCategoryKey = 0x21CA76B1U;"
    "kFilterOperatorEqual = 0;"
    "kIconRowFilterFlags = 0U;"
    "sizeof(AppInfoFilter) == 0x24")
    string(FIND "${abi}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title query ABI invariant is missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "const lsdb::AppInfoFilter filter{"
    "lsdb::kCategoryKey,"
    "{NULL, 0U, 0U},"
    "lsdb::kFilterOperatorEqual,"
    "lsdb::kIconRowFilterFlags"
    "QueryHomeBubbleTitleIds("
    "api_.get_all_app_info(&database, OnAppInfo, &context);")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title query construction/order is missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "Contract ID: `filtered-title-query-semantics-v1`"
    "`sceLsdbGetCategory`"
    "key `0x21CA76B1`"
    "`fun_BuildIconFilteredTitleQuery` at `0x8118924A`"
    "`i.type = 0`"
    "`pageNo == -100000000`"
    "`sub_832CD9C8` at `0x832CD9C8`"
    "type `5` as a multi-icon/special page"
    "type `6` as the power icon"
    "`NpCommIdSearchJob::Run`"
    "It does not use a title-ID or category blacklist")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title semantic evidence is missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "0x10C1A2E0"
    "0xDE0DA538"
    "GetIconInfoByTitleId")
    string(FIND "${abi}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Unrelated/rejected LSDB query key returned to the ABI: ${forbidden}")
    endif()
    string(FIND "${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Unrelated/rejected LSDB query key returned to the caller: ${forbidden}")
    endif()
endforeach()

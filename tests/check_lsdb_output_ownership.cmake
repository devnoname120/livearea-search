if(NOT DEFINED LSDB_SOURCE OR NOT DEFINED LSDB_ABI OR NOT DEFINED OWNERSHIP_REPORT)
    message(FATAL_ERROR "LSDB_SOURCE, LSDB_ABI and OWNERSHIP_REPORT are required")
endif()

file(READ "${LSDB_SOURCE}" source)
file(READ "${LSDB_ABI}" abi)
file(READ "${OWNERSHIP_REPORT}" report)

foreach(required IN ITEMS
    "void ReleaseStringArray(lsdb::StringArray& strings)"
    "strings.data[index].data != NULL && strings.data[index].length != 0U"
    "sce_paf_free(strings.data[index].data);"
    "sce_paf_free(strings.data);"
    "strings.data = NULL;"
    "strings.count = 0U;"
    "strings.capacity = 0U;"
    "api_.get_all_app_info(&database, OnAppInfo, &context);"
    "QueryHomeBubbleTitleIds(")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title output ownership invariant is missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "struct String"
    "struct StringArray"
    "sizeof(String) == 0xC"
    "sizeof(StringArray) == 0xC"
    "using GetFilteredTitleIds = int (*)")
    string(FIND "${abi}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title ABI invariant is missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "Contract ID: `filtered-title-array-ownership-v1`"
    "`fun_QueryFilteredTitleIds` at `0x81189FC6`"
    "NID `0x1B77082E`"
    "`sce_paf_private_malloc` NID `0xFC5CD359`"
    "public `sce_paf_free`"
    "empty-string sentinel"
    "query occurs after enumeration")
    string(FIND "${report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Filtered-title firmware evidence is missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "GetIconInfoByTitleId"
    "0xDE0DA538"
    "IconInfoRef")
    string(FIND "${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Rejected icon-query ownership pattern returned: ${forbidden}")
    endif()
endforeach()

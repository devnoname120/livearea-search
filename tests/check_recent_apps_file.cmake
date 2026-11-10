if(NOT DEFINED RECENT_FILE_HEADER OR NOT DEFINED RECENT_FILE_SOURCE)
    message(FATAL_ERROR "RECENT_FILE_HEADER and RECENT_FILE_SOURCE are required")
endif()

file(READ "${RECENT_FILE_HEADER}" recent_header)
file(READ "${RECENT_FILE_SOURCE}" recent_source)

foreach(required_header IN ITEMS
    "class RecentAppsFile"
    "int Load(RecentApps& recent_apps) const;"
    "int Save(const RecentApps& recent_apps) const;")
    string(FIND "${recent_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing RecentAppsFile API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "ur0:data/livearea-search/recent_apps.txt"
    "ur0:data/livearea-search/recent_apps.txt.tmp"
    "sceIoSyncByFd"
    "VerifyFile"
    "sceIoRename"
    "RecentApps::kMaximumSerializedSize")
    string(FIND "${recent_source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing bounded atomic persistence step: ${required_source}")
    endif()
endforeach()

string(FIND "${recent_source}" "sceIoRemove(kRecentPath);" destructive_remove)
if(NOT destructive_remove EQUAL -1)
    message(FATAL_ERROR "Do not delete the valid destination before replacement")
endif()

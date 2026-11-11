if(NOT DEFINED PLUGIN_MAIN OR
   NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED APP_INDEX_HEADER OR
   NOT DEFINED APP_INDEX_SOURCE OR
   NOT DEFINED RECENT_APPS_HEADER OR
   NOT DEFINED RECENT_APPS_SOURCE)
    message(FATAL_ERROR "All startup contract paths are required")
endif()

file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${APP_INDEX_HEADER}" app_index_header)
file(READ "${APP_INDEX_SOURCE}" app_index_source)
file(READ "${RECENT_APPS_HEADER}" recent_apps_header)
file(READ "${RECENT_APPS_SOURCE}" recent_apps_source)

foreach(required IN ITEMS
    "struct PreparedSearchData"
    "LsdbIndexSource index_source;"
    "RecentAppsFile recent_apps_file;"
    "index_source.Load(prepared->index)"
    "recent_apps_file.Load(prepared->recent_apps)"
    "ClassifyPreparedDataLoadResult(prepare_result)"
    "PreparedDataLoadAction::WaitForDatabase"
    "sceKernelDelayThread(kLsdbReadyPollDelayUs)"
    "while (IsRunning())"
    "g_prepared_search_data"
    "IsTopMenuHomeRenderReady(top_menu)"
    "const std::uint32_t kShellReadyFrameThreshold = 1;"
    "g_overlay->Initialize(prepared->index, prepared->recent_apps)")
    string(FIND "${plugin_main}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing worker-prepared startup behavior: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "sceLsdbOpen"
    "kShellReadyFrameThreshold = 60")
    string(FIND "${plugin_main}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Startup must wait for SceShell's AppDB instead of forcing it: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
    "int Initialize(AppIndex& prepared_index, RecentApps& prepared_recent_apps);")
    string(FIND "${overlay_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing prepared-data overlay API: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "int SearchOverlay::Initialize(AppIndex& prepared_index, RecentApps& prepared_recent_apps)"
    "index_.Swap(prepared_index);"
    "recent_apps_.Swap(prepared_recent_apps);")
    string(FIND "${search_overlay}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing zero-copy UI handoff: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "index_source_.Resolve()"
    "index_source_.Load(index_)"
    "recent_apps_file_.Load(recent_apps_)")
    string(FIND "${search_overlay}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "PAF-thread startup still performs blocking data work: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
    "void Swap(AppIndex& other) noexcept;")
    string(FIND "${app_index_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing AppIndex ownership transfer API: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "void AppIndex::Swap(AppIndex& other) noexcept"
    "apps_.swap(other.apps_);"
    "indexed_apps_.swap(other.indexed_apps_);")
    string(FIND "${app_index_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing AppIndex O(1) ownership transfer: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "void Swap(RecentApps& other) noexcept;")
    string(FIND "${recent_apps_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing RecentApps ownership transfer API: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "void RecentApps::Swap(RecentApps& other) noexcept"
    "title_ids_.swap(other.title_ids_);")
    string(FIND "${recent_apps_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing RecentApps O(1) ownership transfer: ${required}")
    endif()
endforeach()

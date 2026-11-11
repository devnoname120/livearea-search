if(NOT DEFINED SEARCH_OVERLAY OR NOT DEFINED PLUGIN_MAIN)
    message(FATAL_ERROR "SEARCH_OVERLAY and PLUGIN_MAIN are required")
endif()

file(READ "${SEARCH_OVERLAY}" source)
file(READ "${PLUGIN_MAIN}" plugin_main)

foreach(required IN ITEMS
    "recent_apps_.Resolve(index_.apps(), next_results);"
    "SetText(result_count_, \"Recent\");"
    "SetText(empty_text_, \"No recent apps\");"
    "recent_apps_.RecordSelection(title_id);"
    "recent_apps_file_.Save(recent_apps_);"
    "OpenTitleLiveArea(title_id)")
    string(FIND "${source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing Recent overlay behavior: ${required}")
    endif()
endforeach()

string(FIND "${plugin_main}" "recent_apps_file.Load(prepared->recent_apps)" recent_load)
if(recent_load EQUAL -1)
    message(FATAL_ERROR "Recent history must be loaded before the PAF-thread handoff")
endif()

string(FIND "${source}" "recent_apps_.RecordSelection(title_id);" record_index)
string(FIND "${source}" "recent_apps_file_.Save(recent_apps_);" save_index)
string(FIND "${source}" "OpenTitleLiveArea(title_id)" launch_index)
if(NOT record_index LESS save_index OR NOT save_index LESS launch_index)
    message(FATAL_ERROR "Selection must be recorded and persisted before title-open")
endif()

string(FIND "${source}" "ime_close_pending_ = normalized_query_.empty();" empty_close)
if(NOT empty_close EQUAL -1)
    message(FATAL_ERROR "An empty query must show Recent instead of closing")
endif()

string(FIND "${source}" "results_pending_display_ = !normalized_query_.empty();" empty_pending)
if(NOT empty_pending EQUAL -1)
    message(FATAL_ERROR "Recent must remain pending when IME editing ends with an empty query")
endif()

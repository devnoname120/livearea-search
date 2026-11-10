if(NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED ICON_CACHE_HEADER OR
   NOT DEFINED ICON_CACHE_SOURCE)
    message(FATAL_ERROR "SEARCH_OVERLAY, ICON_CACHE_HEADER, and ICON_CACHE_SOURCE are required")
endif()

file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${ICON_CACHE_HEADER}" icon_cache_header)
file(READ "${ICON_CACHE_SOURCE}" icon_cache_source)

foreach(required_source IN ITEMS
    "RequestCloseResultsPage();"
    "ProcessCloseRequest();"
    "result_list_->Clear();"
    "icon_cache_->SetNotificationsEnabled(false);"
    "icon_cache_->ResetPins();"
    "close_param.fade = false;")
    string(FIND "${search_overlay}" "${required_source}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing safe close lifecycle step: ${required_source}")
    endif()
endforeach()

string(FIND "${search_overlay}" "overlay->CloseResultsPage();" direct_callback_close)
if(NOT direct_callback_close EQUAL -1)
    message(FATAL_ERROR "Input callbacks must defer result-page teardown")
endif()

string(FIND "${search_overlay}" "icon_cache_->SetNotificationsEnabled(false);" notifications_off)
string(FIND "${search_overlay}" "result_list_->Clear();" clear_list)
string(FIND "${search_overlay}" "plugin_->PageClose(\"page_search_results\"" close_page)
string(FIND "${search_overlay}" "results_.clear();" clear_model REVERSE)
if(notifications_off EQUAL -1 OR clear_list EQUAL -1 OR close_page EQUAL -1 OR clear_model EQUAL -1 OR
   NOT notifications_off LESS clear_list OR
   NOT clear_list LESS close_page OR
   NOT close_page LESS clear_model)
    message(FATAL_ERROR "Close must quiesce notifications and rows before closing and clearing the model")
endif()

foreach(required_api IN ITEMS
    "void SetNotificationsEnabled(bool enabled);"
    "void ResetPins();"
    "bool notifications_enabled_;")
    string(FIND "${icon_cache_header}" "${required_api}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing icon-cache teardown API: ${required_api}")
    endif()
endforeach()

string(FIND "${icon_cache_source}" "accepted && notifications_enabled" guarded_broadcast)
if(guarded_broadcast EQUAL -1)
    message(FATAL_ERROR "Icon-ready broadcasts must be suppressed while the page is closing")
endif()

string(FIND "${icon_cache_source}" "SetPageClosing" diagnostic_state)
if(NOT diagnostic_state EQUAL -1)
    message(FATAL_ERROR "Diagnostic page-closing state must not ship")
endif()

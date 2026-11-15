if(NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED POLICY_HEADER OR
   NOT DEFINED ICON_CACHE_HEADER OR
   NOT DEFINED ICON_CACHE_SOURCE)
    message(FATAL_ERROR
        "SEARCH_OVERLAY, OVERLAY_HEADER, POLICY_HEADER, ICON_CACHE_HEADER, and ICON_CACHE_SOURCE are required")
endif()

file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${POLICY_HEADER}" policy_header)
file(READ "${ICON_CACHE_HEADER}" icon_cache_header)
file(READ "${ICON_CACHE_SOURCE}" icon_cache_source)

foreach(required_header IN ITEMS
    "ResultsCloseState results_close_state_;"
    "std::uint64_t results_close_deadline_us_;"
    "void RequestCloseResultsPage(ResultsCloseMode mode);"
    "void DisableResultsInput();")
    string(FIND "${overlay_header}" "${required_header}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing results-close header contract: ${required_header}")
    endif()
endforeach()

foreach(required_policy IN ITEMS
    "kResultsBackHideDurationUs ="
    "kHomeCornerHideDurationUs;"
    "enum class ResultsCloseState"
    "PendingAnimated"
    "AnimatingBackCorner"
    "PendingImmediate"
    "StartBackCornerHide")
    string(FIND "${policy_header}" "${required_policy}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing results-close policy contract: ${required_policy}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "RequestCloseResultsPage(ResultsCloseMode::AnimatedBackCorner);"
    "RequestCloseResultsPage(ResultsCloseMode::Immediate);"
    "AdvanceResultsCloseTransition("
    "close_button_->Hide(paf::common::transition::Type_Reset);"
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

foreach(forbidden_source IN ITEMS
    "close_requested_"
    "close_defer_frames_"
    "AdvanceDeferredResultClose(")
    string(FIND "${search_overlay}${overlay_header}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Obsolete frame-only close state must be removed: ${forbidden_source}")
    endif()
endforeach()

string(FIND "${search_overlay}" "overlay->CloseResultsPage();" direct_callback_close)
if(NOT direct_callback_close EQUAL -1)
    message(FATAL_ERROR "Input callbacks must defer result-page teardown")
endif()

string(FIND "${search_overlay}" "void SearchOverlay::DisableResultsInput()" disable_start)
string(FIND "${search_overlay}" "void SearchOverlay::RequestCloseResultsPage(ResultsCloseMode mode)" request_start)
string(FIND "${search_overlay}" "void SearchOverlay::ProcessCloseRequest()" process_start)
string(FIND "${search_overlay}" "void SearchOverlay::CloseResultsPage()" close_start)
string(FIND "${search_overlay}" "void SearchOverlay::SubmitSearch()" submit_start)
if(disable_start EQUAL -1 OR request_start EQUAL -1 OR process_start EQUAL -1 OR
   close_start EQUAL -1 OR submit_start EQUAL -1 OR
   NOT disable_start LESS request_start OR NOT request_start LESS process_start OR
   NOT process_start LESS close_start OR NOT close_start LESS submit_start)
    message(FATAL_ERROR "Unable to isolate results-close lifecycle functions")
endif()

math(EXPR disable_length "${request_start} - ${disable_start}")
string(SUBSTRING "${search_overlay}" ${disable_start} ${disable_length} disable_body)
foreach(required_disable IN ITEMS
    "results_page_->DisableEvent(mask);"
    "results_page_->ReleaseFocus();"
    "search_box_->DisableEvent(mask);"
    "search_box_->ReleaseFocus();"
    "result_list_->DisableEvent(mask);"
    "result_list_->ReleaseFocus();"
    "close_button_->DisableEvent(mask);"
    "close_button_->ReleaseFocus();")
    string(FIND "${disable_body}" "${required_disable}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Closing Search must quiesce all results input: ${required_disable}")
    endif()
endforeach()

math(EXPR request_length "${process_start} - ${request_start}")
string(SUBSTRING "${search_overlay}" ${request_start} ${request_length} request_body)
string(FIND "${request_body}" "DisableResultsInput();" disable_call)
string(FIND "${request_body}" "QueueResultsClose(" queue_call)
if(disable_call EQUAL -1 OR queue_call EQUAL -1 OR NOT disable_call LESS queue_call)
    message(FATAL_ERROR "A close request must disable input before queueing transition state")
endif()

math(EXPR process_length "${close_start} - ${process_start}")
string(SUBSTRING "${search_overlay}" ${process_start} ${process_length} process_body)
string(FIND "${process_body}" "ResultsCloseAction::StartBackCornerHide" start_hide_action)
string(FIND "${process_body}" "close_button_->Hide(paf::common::transition::Type_Reset);" hide_call)
string(FIND "${process_body}" "ResultsCloseAction::Close" close_action)
string(FIND "${process_body}" "CloseResultsPage();" deferred_close_call)
if(start_hide_action EQUAL -1 OR hide_call EQUAL -1 OR close_action EQUAL -1 OR
   deferred_close_call EQUAL -1 OR
   NOT start_hide_action LESS hide_call OR NOT hide_call LESS close_action OR
   NOT close_action LESS deferred_close_call)
    message(FATAL_ERROR "The Back corner must hide before deadline-driven page teardown")
endif()

math(EXPR close_length "${submit_start} - ${close_start}")
string(SUBSTRING "${search_overlay}" ${close_start} ${close_length} close_body)
string(FIND "${close_body}" "ReleaseHomePadLock();" final_unlock)
string(FIND "${close_body}" "plugin_->PageClose(\"page_search_results\"" final_page_close)
if(final_unlock EQUAL -1 OR final_page_close EQUAL -1 OR
   NOT final_unlock LESS final_page_close)
    message(FATAL_ERROR "Final teardown must release the PS lock immediately before PageClose")
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

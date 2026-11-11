if(NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "SEARCH_OVERLAY is required")
endif()

file(READ "${SEARCH_OVERLAY}" source)

string(FIND "${source}"
    "void SearchOverlay::UpdateImeTransition()"
    update_start)
string(FIND "${source}"
    "void SearchOverlay::SetResultsDrawPriority"
    update_end)
if(update_start EQUAL -1 OR update_end EQUAL -1 OR
   NOT update_start LESS update_end)
    message(FATAL_ERROR "Unable to isolate UpdateImeTransition")
endif()

math(EXPR update_length "${update_end} - ${update_start}")
string(SUBSTRING "${source}" ${update_start} ${update_length} update_body)
string(FIND "${update_body}" "SetViewBox(" frame_viewbox)
if(NOT frame_viewbox EQUAL -1)
    message(FATAL_ERROR
        "UpdateImeTransition must not reset the ListView camera every frame")
endif()

set(compact_viewbox
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 112.0f, 480.0f, -96.0f));")
string(REGEX MATCHALL
    "result_list_->SetViewBox\\(paf::math::v4\\(-480\\.0f, 112\\.0f, 480\\.0f, -96\\.0f\\)\\)"
    compact_viewbox_calls
    "${source}")
list(LENGTH compact_viewbox_calls compact_viewbox_call_count)
if(NOT compact_viewbox_call_count EQUAL 1)
    message(FATAL_ERROR
        "The compact camera box must be established exactly once by the editing layout")
endif()

string(FIND "${source}"
    "void SearchOverlay::SetResultsEditingLayout()"
    editing_start)
string(FIND "${source}"
    "void SearchOverlay::SetResultsDisplayLayout()"
    editing_end)
if(editing_start EQUAL -1 OR editing_end EQUAL -1 OR
   NOT editing_start LESS editing_end)
    message(FATAL_ERROR "Unable to isolate SetResultsEditingLayout")
endif()

math(EXPR editing_length "${editing_end} - ${editing_start}")
string(SUBSTRING "${source}" ${editing_start} ${editing_length} editing_body)
string(FIND "${editing_body}" "${compact_viewbox}" editing_viewbox)
if(editing_viewbox EQUAL -1)
    message(FATAL_ERROR
        "SetResultsEditingLayout must establish the exact IME camera box")
endif()

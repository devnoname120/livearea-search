if(NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "SEARCH_OVERLAY is required")
endif()

file(READ "${SEARCH_OVERLAY}" source)

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

foreach(required IN ITEMS
    "results_backdrop_->Show(paf::common::transition::Type_Reset);"
    "results_surface_->Show(paf::common::transition::Type_Reset);"
    "results_ime_surface_->Hide(paf::common::transition::Type_Reset);"
    "result_list_->SetSize(960.0f, 448.0f, 0.0f);"
    "result_list_->SetPos(0.0f, -48.0f, 0.0f);"
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 224.0f, 480.0f, -224.0f));"
    "empty_text_->SetPos(0.0f, -48.0f, 0.0f);")
    string(FIND "${editing_body}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "IME editing must retain the full opaque results composition: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "results_backdrop_->Hide("
    "results_surface_->Hide("
    "results_ime_surface_->Show("
    "result_list_->SetSize(960.0f, 224.0f, 0.0f);"
    "result_list_->SetPos(0.0f, 64.0f, 0.0f);"
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 112.0f, 480.0f, -96.0f));")
    string(FIND "${editing_body}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "IME editing must not expose the compact half-page layout: ${forbidden}")
    endif()
endforeach()

string(FIND "${source}" "SetResultsEditingLayout();" editing_call)
string(FIND "${source}" "const int result = search_box_->BeginEdit();" begin_edit)
if(editing_call EQUAL -1 OR begin_edit EQUAL -1 OR NOT editing_call LESS begin_edit)
    message(FATAL_ERROR "The full result composition must be established before BeginEdit")
endif()

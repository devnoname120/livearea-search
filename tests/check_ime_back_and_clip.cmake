if(NOT DEFINED SEARCH_OVERLAY OR NOT DEFINED OVERLAY_HEADER)
    message(FATAL_ERROR "SEARCH_OVERLAY and OVERLAY_HEADER are required")
endif()

file(READ "${SEARCH_OVERLAY}" source)
file(READ "${OVERLAY_HEADER}" header)

foreach(required_header IN ITEMS
    "void HandleBack();"
    "static void OnBackKey(")
    string(FIND "${header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing two-stage Back API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "paf::inputdevice::pad::Data::Keycode::PAD_ESCAPE"
    "results_page_->SetKeycode("
    "paf::ui::Handler::CB_KEYCODE_PRESS"
    "ComputeResultsBackAction("
    "overlay->HandleBack();")
    string(FIND "${source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing semantic/two-stage Back behavior: ${required_source}")
    endif()
endforeach()

string(REGEX MATCHALL "result_list_->SyncViewSize\\(\\);" sync_calls "${source}")
list(LENGTH sync_calls sync_call_count)
if(sync_call_count LESS 2)
    message(FATAL_ERROR
        "Both IME and expanded ListView geometry changes must synchronize the PAF view box")
endif()

foreach(required_geometry IN ITEMS
    "result_list_->SetSize(960.0f, 224.0f, 0.0f);"
    "result_list_->SetPos(0.0f, 64.0f, 0.0f);"
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 112.0f, 480.0f, -96.0f));"
    "result_list_->SetSize(960.0f, 448.0f, 0.0f);"
    "result_list_->SetPos(0.0f, -48.0f, 0.0f);"
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 224.0f, 480.0f, -224.0f));")
    string(FIND "${source}" "${required_geometry}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "ListView clipping geometry must meet the header and system IME boundaries: ${required_geometry}")
    endif()
endforeach()

string(REGEX MATCHALL
    "result_list_->SetViewBox\\(paf::math::v4\\(-480\\.0f, 112\\.0f, 480\\.0f, -96\\.0f\\)\\)"
    compact_viewbox_calls
    "${source}")
list(LENGTH compact_viewbox_calls compact_viewbox_call_count)
if(compact_viewbox_call_count LESS 2)
    message(FATAL_ERROR
        "The exact IME view box must be set during layout and reasserted after progressive list updates")
endif()

foreach(forbidden_geometry IN ITEMS
    "result_list_->SetSize(960.0f, 192.0f, 0.0f);"
    "result_list_->SetSize(960.0f, 208.0f, 0.0f);")
    string(FIND "${source}" "${forbidden_geometry}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "The compact ListView must use aligned internal geometry with an exact camera view box: ${forbidden_geometry}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "SCE_CTRL_CIRCLE"
    "SCE_CTRL_CROSS"
    "page_ime_boundary_mask"
    "plane_results_ime_mask"
    "paf_widget_clip::"
    "results_clip_")
    string(FIND "${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Back handling must not hard-code a physical face button: ${forbidden}")
    endif()
endforeach()

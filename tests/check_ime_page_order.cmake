if(NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED PAF_PAGE_ORDER_HEADER)
    message(FATAL_ERROR
        "SEARCH_OVERLAY, OVERLAY_HEADER, and PAF_PAGE_ORDER_HEADER are required")
endif()

file(READ "${SEARCH_OVERLAY}" source)
file(READ "${OVERLAY_HEADER}" header)
file(READ "${PAF_PAGE_ORDER_HEADER}" page_order_header)

foreach(required_header IN ITEMS
    "void SetResultsDrawPriority(int priority);"
    "int results_draw_priority_;")
    string(FIND "${header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing IME-aware draw-order state: ${required_header}")
    endif()
endforeach()

foreach(required_adapter IN ITEMS
    "namespace paf_page_order"
    "_ZN3paf6Plugin19SetDrawOrder_RemoveEPNS_2ui5SceneE"
    "_ZN3paf6Plugin19AdjustPageDrawOrderEPNS_2ui5SceneERKNS_7IDParamEi"
    "inline void SetPagePriority("
    "SetDrawOrderRemove(plugin, page);"
    "AdjustPageDrawOrder(plugin, page, id, priority);")
    string(FIND "${page_order_header}" "${required_adapter}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Missing bounded ABI adapter for PAF's exported page-order methods: ${required_adapter}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "const int kResultsDisplayDrawPriority = 6;"
    "const int kResultsEditingDrawPriority = 36;"
    "paf_page_order::SetPagePriority("
    "\"page_search_results\""
    "SetResultsDrawPriority(kResultsEditingDrawPriority);"
    "SetResultsDrawPriority(kResultsDisplayDrawPriority);")
    string(FIND "${source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing firmware-backed IME page ordering: ${required_source}")
    endif()
endforeach()

string(REGEX MATCHALL
    "SetResultsDrawPriority\\(kResultsEditingDrawPriority\\);"
    editing_priority_calls
    "${source}")
list(LENGTH editing_priority_calls editing_priority_call_count)
if(editing_priority_call_count LESS 2)
    message(FATAL_ERROR
        "Both explicit and externally adopted IME edits must move Search behind the IME page")
endif()

string(FIND "${source}"
    "SetResultsDrawPriority(kResultsEditingDrawPriority);"
    editing_priority_index)
string(FIND "${source}"
    "const int result = search_box_->BeginEdit();"
    begin_edit_index)
if(editing_priority_index EQUAL -1 OR begin_edit_index EQUAL -1 OR
   NOT editing_priority_index LESS begin_edit_index)
    message(FATAL_ERROR "Search must move behind the IME before BeginEdit opens the keyboard page")
endif()

string(FIND "${source}"
    "SetResultsDrawPriority(kResultsDisplayDrawPriority);"
    display_priority_index)
string(FIND "${source}"
    "SetResultsDisplayLayout();"
    display_layout_index)
if(display_priority_index EQUAL -1 OR display_layout_index EQUAL -1 OR
   NOT display_priority_index LESS display_layout_index)
    message(FATAL_ERROR "Search must restore its foreground priority before expanded results appear")
endif()

foreach(forbidden IN ITEMS
    "ReadPluginDrawPriority"
    "open_param.overwrite_draw_priority = 36;"
    "page_ime_boundary_mask"
    "OpenImeBoundaryMask"
    "CloseImeBoundaryMask"
    "plugin_->SetDrawOrder_Remove("
    "plugin_->AdjustPageDrawOrder(")
    string(FIND "${source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "IME ordering must use the bounded page-order adapter, not private probes: ${forbidden}")
    endif()
endforeach()

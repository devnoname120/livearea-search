foreach(required_var IN ITEMS ADAPTER_HEADER OVERLAY_HEADER SEARCH_OVERLAY)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} is required")
    endif()
endforeach()

if(NOT EXISTS "${ADAPTER_HEADER}")
    message(FATAL_ERROR "Missing touchscreen ABI adapter: ${ADAPTER_HEADER}")
endif()

file(READ "${ADAPTER_HEADER}" adapter)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" source)

foreach(required_adapter IN ITEMS
    "struct DataRecord"
    "std::int32_t x;"
    "std::int32_t y;"
    "std::int32_t state;"
    "DataRecord records[8];"
    "static_assert(sizeof(DataRecord) == 0x20"
    "static_assert(offsetof(DataRecord, x) == 0x08"
    "static_assert(offsetof(DataRecord, y) == 0x0C"
    "static_assert(offsetof(DataRecord, state) == 0x14"
    "static_assert(sizeof(DataList) == 0x100"
    "_ZN3paf11inputdevice11touchscreen16SetDeviceHandlerEjPFvPNS1_8DataListEE")
    string(FIND "${adapter}" "${required_adapter}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing firmware-proven touchscreen ABI element: ${required_adapter}")
    endif()
endforeach()

foreach(required_header IN ITEMS
    "#include \"paf_touchscreen_device_handler.h\""
    "static void TouchscreenDeviceHandler("
    "static paf_touchscreen_device_handler::DeviceHandler previous_touchscreen_handler_;"
    "NotificationPanelSuppression notification_panel_suppression_;"
    "std::uint64_t notification_panel_opening_deadline_us_;"
    "void SetHomeVisible(bool visible);")
    string(FIND "${overlay_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing notification preemption declaration: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "paf_touchscreen_device_handler::DeviceHandler SearchOverlay::previous_touchscreen_handler_ = NULL;"
    "paf_touchscreen_device_handler::SetDeviceHandler(0U, TouchscreenDeviceHandler)"
    "void SearchOverlay::TouchscreenDeviceHandler("
    "ObserveNotificationCornerPress("
    "AdvanceNotificationPanelSuppression("
    "const int result = indicator->GetOpenedPages(&page_ids);"
    "ClassifyIndicatorPopupVisibility("
    "static_root->FindChild(kIndicatorNotificationCornerHash)"
    "ClassifyIndicatorPopupOwnership("
    "sceKernelGetSystemTimeWide()"
    "notification_panel_opening_deadline_us_"
    "overlay->SetHomeVisible(false);"
    "previous_touchscreen_handler_(data);"
    "touch front slot=%u state=%d x=%d y=%d")
    string(FIND "${source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing notification preemption behavior: ${required_source}")
    endif()
endforeach()

string(FIND "${source}" "if (indicator->GetOpenedPages(&page_ids) < 0)" discarded_vector)
if(NOT discarded_vector EQUAL -1)
    message(FATAL_ERROR "GetOpenedPages output must not be discarded solely because PAF returned its -1 sentinel")
endif()

string(FIND "${overlay_header}" "#if LIVEAREA_SEARCH_DIAGNOSTICS\n#include \"paf_touchscreen_device_handler.h\"\n#endif" guarded_include)
if(NOT guarded_include EQUAL -1)
    message(FATAL_ERROR "The firmware-proven touchscreen adapter must be available in the release profile")
endif()

string(FIND "${source}" "ObserveNotificationCornerPress(" observe_index)
string(FIND "${source}" "overlay->SetHomeVisible(false);" hide_index)
string(FIND "${source}" "previous_touchscreen_handler_(data);" forward_index)
if(observe_index EQUAL -1 OR hide_index EQUAL -1 OR forward_index EQUAL -1 OR
   NOT observe_index LESS hide_index OR NOT hide_index LESS forward_index)
    message(FATAL_ERROR "Home Search must be hidden before Sony's touchscreen handler receives the notification press")
endif()

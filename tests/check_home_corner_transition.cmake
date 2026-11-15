if(NOT DEFINED POLICY_HEADER OR NOT DEFINED OVERLAY_HEADER OR NOT DEFINED SEARCH_OVERLAY OR NOT DEFINED REPORT)
    message(FATAL_ERROR "POLICY_HEADER, OVERLAY_HEADER, SEARCH_OVERLAY, and REPORT are required")
endif()

file(READ "${POLICY_HEADER}" policy_header)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${REPORT}" report)

foreach(required_header IN ITEMS
    "HomeCornerTransitionState home_corner_transition_state_;"
    "std::uint64_t home_corner_transition_deadline_us_;"
    "void UpdateHomeCornerTransition();")
    string(FIND "${overlay_header}" "${required_header}" required_header_index)
    if(required_header_index EQUAL -1)
        message(FATAL_ERROR "Missing Home corner transition header contract: ${required_header}")
    endif()
endforeach()

foreach(required_policy IN ITEMS
    "kHomeCornerShowDurationUs = 350000"
    "kHomeCornerHideDurationUs = 150000"
    "HomeCornerTransitionCompletion AdvanceHomeCornerTransition(")
    string(FIND "${policy_header}" "${required_policy}" required_policy_index)
    if(required_policy_index EQUAL -1)
        message(FATAL_ERROR "Missing Home corner transition policy contract: ${required_policy}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "RequestHomeCornerTransition("
    "AdvanceHomeCornerTransition("
    "home_button_->Show(paf::common::transition::Type_Reset);"
    "home_button_->Hide(paf::common::transition::Type_Reset);"
    "home_page_->Show(paf::common::transition::Type_Reset);"
    "home_page_->Hide(paf::common::transition::Type_Reset);"
    "home_button_->DisableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);"
    "home_button_->EnableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);")
    string(FIND "${search_overlay}" "${required_source}" required_source_index)
    if(required_source_index EQUAL -1)
        message(FATAL_ERROR "Missing native Home corner transition source contract: ${required_source}")
    endif()
endforeach()

foreach(forbidden_source IN ITEMS
    "home_page_->Show(paf::common::transition::Type_FadeinFast);"
    "home_page_->Hide(paf::common::transition::Type_FadeinFast);")
    string(FIND "${search_overlay}" "${forbidden_source}" forbidden_source_index)
    if(NOT forbidden_source_index EQUAL -1)
        message(FATAL_ERROR "Search still fades its whole Home page: ${forbidden_source}")
    endif()
endforeach()

foreach(required_report IN ITEMS
    "Contract ID: `home-corner-native-transition-v1`"
    "timer function  32 (Timer::FUNC_EXPO_INOUT"
    "timer duration  349 ms"
    "animation ID    0x00010005"
    "timer function  7 (Timer::FUNC_QUAD_IN)"
    "timer duration  149 ms"
    "animation ID    0x00010004"
    "show alpha   1.000")
    string(FIND "${report}" "${required_report}" required_report_index)
    if(required_report_index EQUAL -1)
        message(FATAL_ERROR "Missing firmware-backed Home corner evidence: ${required_report}")
    endif()
endforeach()

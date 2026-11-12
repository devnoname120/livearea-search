foreach(required_variable IN ITEMS PLUGIN_MAIN SEARCH_OVERLAY REPORT)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${REPORT}" report)

foreach(required_source IN ITEMS
    "AppMgrAppStateLayout"
    "sizeof(SceAppMgrAppState) == sizeof(livearea_search::AppMgrAppStateLayout)"
    "ShouldAllowShellOverlay("
    "state_layout.is_system_ui_overlaid != 0U")
    string(FIND "${plugin_main}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing AppMgr overlay-state integration: ${required_source}")
    endif()
endforeach()

foreach(forbidden_source IN ITEMS
    "paf::Plugin::Find(\"impose_plugin\")"
    "paf::Plugin::Find(\"impose_net_plugin\")")
    string(FIND "${search_overlay}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Coarse PAF quick-menu visibility gate remains: ${forbidden_source}")
    endif()
endforeach()

foreach(required_evidence IN ITEMS
    "0x210C0046"
    "0x024F2EE0"
    "0x024D14D4"
    "0x810477A4"
    "0x810250C8"
    "0x81032EC8"
    "0x810114D4"
    "offset `0x08`"
    "0x80 bytes")
    string(FIND "${report}" "${required_evidence}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing AppMgr firmware evidence: ${required_evidence}")
    endif()
endforeach()

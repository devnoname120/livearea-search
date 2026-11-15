if(NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED VISIBILITY_HEADER OR
   NOT DEFINED FIRMWARE_REPORT)
    message(FATAL_ERROR "Application-switcher contract paths are required")
endif()

file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${VISIBILITY_HEADER}" visibility_header)
file(READ "${FIRMWARE_REPORT}" firmware_report)

foreach(required IN ITEMS
    "const std::uint32_t kShellLibraryNid = 0x2404AE25U;"
    "const std::uint32_t kShellEventRegisterNid = 0x7FE4E732U;"
    "const std::uint32_t kShellEventUnregisterNid = 0xD4C7D2B7U;"
    "ResolveShellEventFunction("
    "shell_event_register_(\n        kShellEventApplicationSwitcherEnter,"
    "shell_event_register_(\n        kShellEventApplicationSwitcherLeave,"
    "shell_event_unregister_(\n            kShellEventApplicationSwitcherLeave,"
    "shell_event_unregister_(\n            kShellEventApplicationSwitcherEnter,"
    "ClassifyApplicationSwitcherEvent(event)"
    "application_switcher_owned_ = true;"
    "application_switcher_owned_ = false;"
    "SetHomeVisible(false, false);"
    "home_edit_mode_visible,\n        application_switcher_owned_);"
    "HomeCornerTransitionAction::HideNative")
    string(FIND "${search_overlay}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing application-switcher implementation contract: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "static void ShellEventHandler("
    "int ResolveShellEvents();"
    "int RegisterShellEvents();"
    "void UnregisterShellEvents();"
    "bool application_switcher_owned_;"
    "bool shell_events_registered_;"
    "ShellEventRegistrationFunction shell_event_register_;"
    "ShellEventRegistrationFunction shell_event_unregister_;")
    string(FIND "${overlay_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing application-switcher header contract: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "kShellEventApplicationSwitcherEnter = 4;"
    "kShellEventApplicationSwitcherLeave = 5;"
    "ApplicationSwitcherEventAction"
    "ClassifyApplicationSwitcherEvent("
    "bool application_switcher_owned)")
    string(FIND "${visibility_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing application-switcher policy contract: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
    "Contract ID: `shell-application-switcher-events-v1`"
    "`0x7FE4E732`"
    "`0xD4C7D2B7`"
    "`handler(event, argument_2, argument_3, user_data)`"
    "event `4`"
    "event `5`"
    "All 45 locally available SceShell images"
    "does not access that field")
    string(FIND "${firmware_report}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing application-switcher firmware evidence: ${required}")
    endif()
endforeach()

string(FIND "${search_overlay}" "result = RegisterShellEvents();" register_index)
string(FIND "${search_overlay}" "result = OpenHomePage();" open_home_index)
if(register_index EQUAL -1 OR open_home_index EQUAL -1 OR
   NOT register_index LESS open_home_index)
    message(FATAL_ERROR "Shell switcher events must be registered before Home Search is exposed")
endif()

foreach(forbidden IN ITEMS
    "0x9F7C030D"
    "application_switcher_page_hash"
    "LogEnvironmentWidgetDeltas"
    "GetSceneNum("
    "dword_815AC4CC"
    "+ 132")
    string(FIND "${search_overlay}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Rejected switcher inference returned: ${forbidden}")
    endif()
endforeach()

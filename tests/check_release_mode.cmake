if(NOT DEFINED ROOT_CMAKE OR
   NOT DEFINED VITA_CMAKE OR
   NOT DEFINED PLUGIN_MAIN OR
   NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED STARTUP_LOG_HEADER OR
   NOT DEFINED SHELL_STATE_HEADER)
    message(FATAL_ERROR "All release-mode contract paths are required")
endif()

file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${VITA_CMAKE}" vita_cmake)
file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${STARTUP_LOG_HEADER}" startup_log_header)
file(READ "${SHELL_STATE_HEADER}" shell_state_header)

foreach(required IN ITEMS
    "option(LIVEAREA_SEARCH_DIAGNOSTICS \"Enable file/TTY diagnostics in the Vita plugin\" OFF)"
    "option(LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE \"Disable the Vita plugin config entry before experimental startup\" OFF)"
    "LIVEAREA_SEARCH_DIAGNOSTICS=$<BOOL:"
    "LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=$<BOOL:"
    "if(LIVEAREA_SEARCH_DIAGNOSTICS)"
    "list(APPEND LIVEAREA_SEARCH_VITA_SOURCES src/vita/startup_log.cpp)")
    string(FIND "${root_cmake}${vita_cmake}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing release-mode build contract: ${required}")
    endif()
endforeach()

foreach(header IN ITEMS startup_log_header shell_state_header)
    foreach(required IN ITEMS
        "#if LIVEAREA_SEARCH_DIAGNOSTICS"
        "#else"
        "inline void")
        string(FIND "${${header}}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "${header} does not compile diagnostics out: ${required}")
        endif()
    endforeach()
endforeach()

foreach(required IN ITEMS
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\n#include <livearea_search/deferred_log_queue.h>"
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\nlivearea_search::DeferredLogQueue g_interaction_logs;"
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\nvoid FlushInteractionLogs()"
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\nvoid QueueInteractionLog("
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\n    livearea_search::ResetStartupLog();")
    string(FIND "${plugin_main}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Release-only diagnostics are not bounded: ${required}")
    endif()
endforeach()

string(FIND "${search_overlay}"
    "#if LIVEAREA_SEARCH_DIAGNOSTICS\n    char line[96];"
    formatted_interaction_log)
if(formatted_interaction_log EQUAL -1)
    message(FATAL_ERROR "Formatted interaction diagnostics are not compiled out")
endif()

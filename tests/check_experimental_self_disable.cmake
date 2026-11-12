if(NOT DEFINED ROOT_CMAKE OR
   NOT DEFINED VITA_CMAKE OR
   NOT DEFINED PLUGIN_MAIN OR
   NOT DEFINED BUILD_SCRIPT)
    message(FATAL_ERROR "All experimental self-disable contract paths are required")
endif()

file(READ "${ROOT_CMAKE}" root_cmake)
file(READ "${VITA_CMAKE}" vita_cmake)
file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${BUILD_SCRIPT}" build_script)

foreach(required IN ITEMS
    "option(LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE \"Disable the Vita plugin config entry before experimental startup\" OFF)"
    "if(LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE)"
    "list(APPEND LIVEAREA_SEARCH_VITA_SOURCES src/vita/config_file_failsafe.cpp)"
    "if(LIVEAREA_SEARCH_DIAGNOSTICS)\n    set(LIVEAREA_SEARCH_VITA_OPTIMIZATION -Os)"
    "LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=$<BOOL:"
    "#if LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE\n#include \"config_file_failsafe.h\""
    "#if LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE\n    const int disabled_entries ="
    "DisablePluginConfigEntriesForNextBoot();"
    "EXPERIMENTAL_SELF_DISABLE=\${LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE:-OFF}"
    "-DLIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=\"$LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE\"")
    string(FIND "${root_cmake}${vita_cmake}${plugin_main}${build_script}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing experimental self-disable contract: ${required}")
    endif()
endforeach()

string(FIND "${plugin_main}"
    "DisablePluginConfigEntriesForNextBoot();"
    disable_index)
string(FIND "${plugin_main}"
    "if (!PathExists(kResourcePath))"
    resource_index)
if(disable_index EQUAL -1 OR resource_index EQUAL -1 OR
   NOT disable_index LESS resource_index)
    message(FATAL_ERROR "Experimental self-disable must run before resource/PAF initialization")
endif()

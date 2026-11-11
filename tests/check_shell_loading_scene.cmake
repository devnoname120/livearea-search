if(NOT DEFINED PLUGIN_MAIN OR NOT DEFINED MODULE_READINESS_HEADER)
    message(FATAL_ERROR "PLUGIN_MAIN and MODULE_READINESS_HEADER are required")
endif()

file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${MODULE_READINESS_HEADER}" readiness_header)

foreach(required_header IN ITEMS
    "const int kShellLoadingSceneEntry = 25;"
    "const std::int32_t kShellLoadingSceneMissingError = -2135998193;"
    "bool IsShellLoadingSceneDismissed(")
    string(FIND "${readiness_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing firmware-backed loading-scene API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "paf::Framework::Instance()"
    "GetEnvironmentInstance()"
    "GetSceneNum(livearea_search::kShellLoadingSceneEntry)"
    "livearea_search::IsShellLoadingSceneDismissed("
    "shell_loading_scene_ready")
    string(FIND "${plugin_main}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing production loading-scene gate: ${required_source}")
    endif()
endforeach()

string(FIND "${plugin_main}" "GetSceneNum(livearea_search::kShellLoadingSceneEntry)" scene_query)
string(FIND "${plugin_main}" "paf::Plugin::LoadSync(init_param);" load_sync)
if(scene_query EQUAL -1 OR load_sync EQUAL -1 OR NOT scene_query LESS load_sync)
    message(FATAL_ERROR "The shell loading scene must be checked before loading Search")
endif()

foreach(forbidden IN ITEMS
    "LIVEAREA_SEARCH_COLDBOOT_DIAGNOSTIC"
    "LIVEAREA_SEARCH_TOPMENU_LOADING_DIAGNOSTIC"
    "LIVEAREA_SEARCH_SHELL_LOADING_SCENE_DIAGNOSTIC"
    "ColdBootCoverDismissed"
    "ProbeTopMenuLoadingSurface"
    "shell loading scene frame="
    "sceKernelGetProcessTimeWide")
    string(FIND "${plugin_main}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Release startup retains diagnostic-only loading logic: ${forbidden}")
    endif()
endforeach()

if(NOT DEFINED PLUGIN_MAIN)
    message(FATAL_ERROR "PLUGIN_MAIN is required")
endif()

file(READ "${PLUGIN_MAIN}" plugin_main)

string(FIND "${plugin_main}" "bool PathExists(const char* path)" path_exists_start)
string(FIND "${plugin_main}" "sceIoGetstat(path, &stat) >= 0" path_exists_stat)
if(path_exists_start EQUAL -1 OR path_exists_stat EQUAL -1 OR
   NOT path_exists_start LESS path_exists_stat)
    message(FATAL_ERROR "The RCO presence gate must use sceIoGetstat")
endif()

string(FIND "${plugin_main}" "extern \"C\" int module_start" module_start)
string(FIND "${plugin_main}" "extern \"C\" int module_stop" module_stop)
if(module_start EQUAL -1 OR module_stop EQUAL -1 OR
   NOT module_start LESS module_stop)
    message(FATAL_ERROR "The module lifecycle functions are unavailable")
endif()

math(EXPR module_start_length "${module_stop} - ${module_start}")
string(SUBSTRING
    "${plugin_main}"
    ${module_start}
    ${module_start_length}
    module_start_body)

string(FIND "${module_start_body}"
    "if (!PathExists(kResourcePath))"
    resource_check)
string(FIND "${module_start_body}"
    "sceClibMemset(&g_state_mutex"
    state_initialization)
string(FIND "${module_start_body}"
    "sceKernelCreateLwMutex("
    mutex_creation)
string(FIND "${module_start_body}"
    "sceKernelCreateThread("
    worker_creation)
if(resource_check EQUAL -1 OR state_initialization EQUAL -1 OR
   mutex_creation EQUAL -1 OR worker_creation EQUAL -1 OR
   NOT resource_check LESS state_initialization OR
   NOT state_initialization LESS mutex_creation OR
   NOT mutex_creation LESS worker_creation)
    message(FATAL_ERROR
        "The RCO presence gate must run before state, mutex, and worker initialization")
endif()

math(EXPR resource_block_length "${state_initialization} - ${resource_check}")
string(SUBSTRING
    "${module_start_body}"
    ${resource_check}
    ${resource_block_length}
    resource_failure_block)
string(FIND "${resource_failure_block}"
    "return SCE_KERNEL_START_NO_RESIDENT;"
    no_resident_return)
if(no_resident_return EQUAL -1)
    message(FATAL_ERROR "A missing RCO must return SCE_KERNEL_START_NO_RESIDENT")
endif()

foreach(forbidden IN ITEMS
    "sceKernelCreateLwMutex("
    "sceKernelCreateThread("
    "paf::Plugin::LoadSync("
    "g_running = true")
    string(FIND "${resource_failure_block}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Missing-RCO handling performs unsafe initialization: ${forbidden}")
    endif()
endforeach()

string(FIND "${module_start_body}"
    "DisablePluginConfigEntriesForNextBoot();"
    self_disable)
if(NOT self_disable EQUAL -1 AND NOT self_disable LESS resource_check)
    message(FATAL_ERROR
        "Experimental self-disable must precede the missing-RCO return")
endif()

if(NOT DEFINED PLUGIN_MAIN OR NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "PLUGIN_MAIN and SEARCH_OVERLAY are required")
endif()

file(READ "${PLUGIN_MAIN}" plugin_main)
file(READ "${SEARCH_OVERLAY}" search_overlay)

string(FIND "${plugin_main}" "init_param.draw_priority = 6;" plugin_priority_index)
if(plugin_priority_index EQUAL -1)
    message(FATAL_ERROR "The overlay plugin must retain the proven shell draw priority")
endif()

string(REGEX MATCHALL "open_param\\.overwrite_draw_priority = 6" priority_six_pages "${search_overlay}")
list(LENGTH priority_six_pages priority_six_count)
if(NOT priority_six_count EQUAL 1)
    message(FATAL_ERROR "Only the Home control page may use shell draw priority 6")
endif()

string(FIND "${search_overlay}" "open_param.overwrite_draw_priority = 5;" priority_five_index)
if(NOT priority_five_index EQUAL -1)
    message(FATAL_ERROR "Page priority cannot cross the system IME plugin boundary")
endif()

string(FIND "${search_overlay}" "open_param.overwrite_draw_priority = 7;" excessive_priority_index)
if(NOT excessive_priority_index EQUAL -1)
    message(FATAL_ERROR "Results priority 7 obscures the system IME")
endif()

string(FIND "${search_overlay}" "ReadPluginDrawPriority" private_layout_probe_index)
if(NOT private_layout_probe_index EQUAL -1)
    message(FATAL_ERROR "Private PAF object-layout probes must not ship")
endif()

string(FIND "${search_overlay}" "SetResultsEditingLayout();" editing_layout_index)
if(editing_layout_index EQUAL -1)
    message(FATAL_ERROR "IME editing must switch to the compact non-overlapping result layout")
endif()

string(FIND "${search_overlay}" "ComputeImeFrameAction(" transition_policy_index)
if(transition_policy_index EQUAL -1)
    message(FATAL_ERROR "IME completion must use the tested transition policy")
endif()

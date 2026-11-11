if(NOT DEFINED PLUGIN_MAIN)
    message(FATAL_ERROR "PLUGIN_MAIN is required")
endif()

file(READ "${PLUGIN_MAIN}" source)

string(FIND "${source}" "init_param.draw_priority = 36;" editing_view_priority)
if(editing_view_priority EQUAL -1)
    message(FATAL_ERROR
        "The LiveArea Search plugin view must load behind ime_plugin and in front of topmenu")
endif()

string(FIND "${source}" "init_param.draw_priority = 6;" foreground_view_priority)
if(NOT foreground_view_priority EQUAL -1)
    message(FATAL_ERROR
        "The plugin-level view must not remain in front of ime_plugin")
endif()

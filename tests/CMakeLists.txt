add_executable(livearea_search_tests
    test_app_index.cpp
)

target_link_libraries(livearea_search_tests PRIVATE livearea_search_core)
target_compile_features(livearea_search_tests PRIVATE cxx_std_11)

target_compile_options(livearea_search_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(NAME livearea_search_tests COMMAND livearea_search_tests)

add_executable(livearea_search_home_pad_filter_tests
    test_home_pad_filter.cpp
)

target_link_libraries(livearea_search_home_pad_filter_tests PRIVATE livearea_search_core)
target_compile_features(livearea_search_home_pad_filter_tests PRIVATE cxx_std_11)

target_compile_options(livearea_search_home_pad_filter_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(
    NAME livearea_search_home_pad_filter_tests
    COMMAND livearea_search_home_pad_filter_tests
)

add_executable(livearea_search_recent_apps_tests
    test_recent_apps.cpp
)

target_link_libraries(livearea_search_recent_apps_tests PRIVATE livearea_search_core)
target_compile_features(livearea_search_recent_apps_tests PRIVATE cxx_std_11)

target_compile_options(livearea_search_recent_apps_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(
    NAME livearea_search_recent_apps_tests
    COMMAND livearea_search_recent_apps_tests
)

add_test(
    NAME livearea_search_recent_file_contract
    COMMAND ${CMAKE_COMMAND}
        -DRECENT_FILE_HEADER=${CMAKE_SOURCE_DIR}/src/vita/recent_apps_file.h
        -DRECENT_FILE_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/recent_apps_file.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_recent_apps_file.cmake
)

add_test(
    NAME livearea_search_recent_overlay
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_recent_overlay.cmake
)

add_test(
    NAME livearea_search_fullscreen_results
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCE_XML=${CMAKE_SOURCE_DIR}/resources/livearea_search_plugin.xml
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_fullscreen_results.cmake
)

add_test(
    NAME livearea_search_home_control
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCE_XML=${CMAKE_SOURCE_DIR}/resources/livearea_search_plugin.xml
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_home_search_control.cmake
)

add_test(
    NAME livearea_search_experience_contract
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCE_XML=${CMAKE_SOURCE_DIR}/resources/livearea_search_plugin.xml
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DICON_CACHE_HEADER=${CMAKE_SOURCE_DIR}/src/vita/icon_cache.h
        -DICON_CACHE_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/icon_cache.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_search_experience.cmake
)

add_test(
    NAME livearea_search_paf_composition
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_paf_composition.cmake
)

add_test(
    NAME livearea_search_close_lifecycle
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DICON_CACHE_HEADER=${CMAKE_SOURCE_DIR}/src/vita/icon_cache.h
        -DICON_CACHE_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/icon_cache.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_close_lifecycle.cmake
)

add_test(
    NAME livearea_search_notification_corner
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCE_XML=${CMAKE_SOURCE_DIR}/resources/livearea_search_plugin.xml
        -DNOTIFICATION_HEADER=${CMAKE_SOURCE_DIR}/src/vita/notification_corner.h
        -DNOTIFICATION_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/notification_corner.cpp
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_notification_corner.cmake
)

add_test(
    NAME livearea_search_ime_back_and_clip
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ime_back_and_clip.cmake
)

add_test(
    NAME livearea_search_ime_page_order
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DPAF_PAGE_ORDER_HEADER=${CMAKE_SOURCE_DIR}/src/vita/paf_page_order.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ime_page_order.cmake
)

add_test(
    NAME livearea_search_ime_viewbox_lifecycle
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ime_viewbox_lifecycle.cmake
)

add_test(
    NAME livearea_search_plugin_view_order
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_plugin_view_order.cmake
)

add_test(
    NAME livearea_search_ime_full_layout
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ime_full_layout.cmake
)

add_test(
    NAME livearea_search_results_open_transition
    COMMAND ${CMAKE_COMMAND}
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_results_open_transition.cmake
)

add_test(
    NAME livearea_search_polish_contract
    COMMAND ${CMAKE_COMMAND}
        -DRESOURCE_XML=${CMAKE_SOURCE_DIR}/resources/livearea_search_plugin.xml
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DPAF_TEXT_BOX_IME_HEADER=${CMAKE_SOURCE_DIR}/src/vita/paf_text_box_ime.h
        -DPAF_PAD_HANDLER_HEADER=${CMAKE_SOURCE_DIR}/src/vita/paf_pad_device_handler.h
        -DVITA_CMAKE=${CMAKE_SOURCE_DIR}/cmake/VitaShellPlugin.cmake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_search_polish.cmake
)

add_test(
    NAME livearea_search_nonblocking_startup
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DAPP_INDEX_HEADER=${CMAKE_SOURCE_DIR}/include/livearea_search/app_index.h
        -DAPP_INDEX_SOURCE=${CMAKE_SOURCE_DIR}/src/core/app_index.cpp
        -DRECENT_APPS_HEADER=${CMAKE_SOURCE_DIR}/include/livearea_search/recent_apps.h
        -DRECENT_APPS_SOURCE=${CMAKE_SOURCE_DIR}/src/core/recent_apps.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_nonblocking_startup.cmake
)

add_test(
    NAME livearea_search_shell_loading_scene
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DMODULE_READINESS_HEADER=${CMAKE_SOURCE_DIR}/include/livearea_search/module_readiness.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_shell_loading_scene.cmake
)

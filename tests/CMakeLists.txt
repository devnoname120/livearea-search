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

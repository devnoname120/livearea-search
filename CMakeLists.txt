cmake_minimum_required(VERSION 3.20)

project(livearea_search LANGUAGES C CXX)

option(LIVEAREA_SEARCH_BUILD_TESTS "Build host tests" ON)
option(LIVEAREA_SEARCH_BUILD_VITA "Build the SceShell SUPRX" OFF)

add_library(livearea_search_core STATIC
    src/core/app_index.cpp
    src/core/catalog_record.cpp
    src/core/config_failsafe.cpp
    src/core/deferred_log_queue.cpp
    src/core/icon_cache_policy.cpp
    src/core/launch_uri.cpp
    src/core/module_readiness.cpp
    src/core/overlay_visibility.cpp
    src/core/recent_apps.cpp
)

target_include_directories(livearea_search_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(livearea_search_core PUBLIC cxx_std_11)

target_compile_options(livearea_search_core
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

if(LIVEAREA_SEARCH_BUILD_TESTS AND NOT LIVEAREA_SEARCH_BUILD_VITA)
    include(CTest)
    add_subdirectory(tests)
endif()

if(LIVEAREA_SEARCH_BUILD_VITA)
    include(cmake/VitaShellPlugin.cmake)
endif()

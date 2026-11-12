if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "VITASDK is required for LIVEAREA_SEARCH_BUILD_VITA")
endif()

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")

set(
    LIVEAREA_SEARCH_PAF_ROOT
    "$ENV{VITASDK}/arm-vita-eabi"
    CACHE PATH
    "Root containing the full ScePaf-RE include and lib directories"
)

set(LIVEAREA_SEARCH_PAF_INCLUDE "${LIVEAREA_SEARCH_PAF_ROOT}/include")
set(
    LIVEAREA_SEARCH_PAF_LIBRARY
    "${LIVEAREA_SEARCH_PAF_ROOT}/lib/libScePaf_stub_weak.a"
)

if(NOT EXISTS "${LIVEAREA_SEARCH_PAF_INCLUDE}/paf.h")
    message(FATAL_ERROR
        "LIVEAREA_SEARCH_PAF_ROOT must provide include/paf.h from ScePaf-RE")
endif()

if(NOT EXISTS "${LIVEAREA_SEARCH_PAF_LIBRARY}")
    message(FATAL_ERROR
        "LIVEAREA_SEARCH_PAF_ROOT must provide lib/libScePaf_stub_weak.a")
endif()

target_compile_definitions(livearea_search_core PUBLIC
    LIVEAREA_SEARCH_PAF_TYPES=1
)

target_compile_options(livearea_search_core PRIVATE
    -fno-exceptions
    -fno-rtti
    -fshort-wchar
    "-D__declspec(x)="
)

target_include_directories(livearea_search_core SYSTEM BEFORE PRIVATE
    "${LIVEAREA_SEARCH_PAF_INCLUDE}"
)

set(LIVEAREA_SEARCH_VITA_SOURCES
    src/vita/icon_cache.cpp
    src/vita/launcher.cpp
    src/vita/libc_shim.c
    src/vita/lsdb_index_source.cpp
    src/vita/notification_corner.cpp
    src/vita/paf_runtime.cpp
    src/vita/plugin_main.cpp
    src/vita/recent_apps_file.cpp
    src/vita/search_overlay.cpp
    src/vita/widget_event.cpp
)

if(LIVEAREA_SEARCH_DIAGNOSTICS)
    list(APPEND LIVEAREA_SEARCH_VITA_SOURCES src/vita/startup_log.cpp)
endif()

if(LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE)
    list(APPEND LIVEAREA_SEARCH_VITA_SOURCES src/vita/config_file_failsafe.cpp)
endif()

add_executable(livearea_search_vita ${LIVEAREA_SEARCH_VITA_SOURCES})

if(LIVEAREA_SEARCH_DIAGNOSTICS)
    set(LIVEAREA_SEARCH_VITA_OPTIMIZATION -Os)
else()
    set(LIVEAREA_SEARCH_VITA_OPTIMIZATION -O2)
endif()

set_source_files_properties(src/vita/icon_cache.cpp PROPERTIES
    COMPILE_OPTIONS -fno-strict-aliasing
)

set_target_properties(livearea_search_vita PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    LINKER_LANGUAGE C
    OUTPUT_NAME livearea_search
)

target_compile_definitions(livearea_search_vita PRIVATE
    LIVEAREA_SEARCH_VITASDK=1
    LIVEAREA_SEARCH_DIAGNOSTICS=$<BOOL:${LIVEAREA_SEARCH_DIAGNOSTICS}>
    LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE=$<BOOL:${LIVEAREA_SEARCH_EXPERIMENTAL_SELF_DISABLE}>
)

target_compile_options(livearea_search_vita PRIVATE
    ${LIVEAREA_SEARCH_VITA_OPTIMIZATION}
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -fno-builtin
    -fshort-wchar
    -ffunction-sections
    -fdata-sections
    "-D__declspec(x)="
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

target_link_options(livearea_search_vita PRIVATE
    -nostdlib
    -nostartfiles
    -Wl,-q
    -Wl,--gc-sections
    -Wl,--undefined=module_start
    -Wl,--undefined=module_stop
    -Wl,--no-wchar-size-warning
)

target_include_directories(livearea_search_vita SYSTEM BEFORE PRIVATE
    "${LIVEAREA_SEARCH_PAF_INCLUDE}"
)

target_include_directories(livearea_search_vita PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vita
)

target_link_libraries(livearea_search_vita PRIVATE
    livearea_search_core
    "${LIVEAREA_SEARCH_PAF_LIBRARY}"
    SceLibKernel_stub
    SceSysmem_stub
    SceKernelThreadMgr_stub
    SceIofilemgr_stub
    SceVshBridge_stub
    SceAppMgr_stub_weak
    taihen_stub
    gcc
)

vita_create_self(livearea_search.suprx livearea_search_vita
    UNSAFE
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/exports.yml
)

find_program(PSP2CXML psp2cxml-tool REQUIRED)
set(LIVEAREA_SEARCH_RCO_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")
set(LIVEAREA_SEARCH_RCO_XML
    "${LIVEAREA_SEARCH_RCO_DIR}/livearea_search_plugin.xml")
set(LIVEAREA_SEARCH_RCO
    "${LIVEAREA_SEARCH_RCO_DIR}/livearea_search_plugin.rco")

file(MAKE_DIRECTORY "${LIVEAREA_SEARCH_RCO_DIR}")

add_custom_command(
    OUTPUT "${LIVEAREA_SEARCH_RCO}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LIVEAREA_SEARCH_RCO_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/livearea_search_plugin.xml"
        "${LIVEAREA_SEARCH_RCO_XML}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${LIVEAREA_SEARCH_RCO_DIR}/texture"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/texture/search.png"
        "${LIVEAREA_SEARCH_RCO_DIR}/texture/search.png"
    COMMAND "${PSP2CXML}" "${LIVEAREA_SEARCH_RCO_XML}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/livearea_search_plugin.xml"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/texture/search.png"
    WORKING_DIRECTORY "${LIVEAREA_SEARCH_RCO_DIR}"
    VERBATIM
)

add_custom_target(livearea_search_rco ALL
    DEPENDS "${LIVEAREA_SEARCH_RCO}"
)

add_custom_target(livearea_search_dist ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/dist/ur0/data/livearea-search"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/livearea_search.suprx"
        "${CMAKE_CURRENT_BINARY_DIR}/dist/livearea_search.suprx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIVEAREA_SEARCH_RCO}"
        "${CMAKE_CURRENT_BINARY_DIR}/dist/ur0/data/livearea-search/livearea_search_plugin.rco"
    DEPENDS livearea_search.suprx-self livearea_search_rco
    VERBATIM
)

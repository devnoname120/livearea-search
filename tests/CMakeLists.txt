find_package(Python3 COMPONENTS Interpreter REQUIRED)

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

add_executable(livearea_search_exclusive_home_visibility_tests
    test_exclusive_home_visibility.cpp
)

target_link_libraries(
    livearea_search_exclusive_home_visibility_tests
    PRIVATE
        livearea_search_core
)

target_compile_features(
    livearea_search_exclusive_home_visibility_tests
    PRIVATE
        cxx_std_11
)

target_compile_options(
    livearea_search_exclusive_home_visibility_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(
    NAME livearea_search_exclusive_home_visibility_tests
    COMMAND livearea_search_exclusive_home_visibility_tests
)

add_executable(livearea_search_notification_panel_gate_tests
    test_notification_panel_gate.cpp
)

target_link_libraries(
    livearea_search_notification_panel_gate_tests
    PRIVATE
        livearea_search_core
)

target_compile_features(
    livearea_search_notification_panel_gate_tests
    PRIVATE
        cxx_std_11
)

target_compile_options(
    livearea_search_notification_panel_gate_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(
    NAME livearea_search_notification_panel_gate_tests
    COMMAND livearea_search_notification_panel_gate_tests
)

add_executable(livearea_search_appmgr_state_tests
    test_appmgr_state.cpp
)

target_link_libraries(livearea_search_appmgr_state_tests PRIVATE livearea_search_core)
target_compile_features(livearea_search_appmgr_state_tests PRIVATE cxx_std_11)

target_compile_options(livearea_search_appmgr_state_tests
    PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

add_test(
    NAME livearea_search_appmgr_state_tests
    COMMAND livearea_search_appmgr_state_tests
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
    NAME livearea_search_appmgr_overlay_state
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DREPORT=${CMAKE_SOURCE_DIR}/docs/firmware/appmgr-system-ui-overlay-state.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_appmgr_overlay_state.cmake
)

add_test(
    NAME livearea_search_touchscreen_notification_gate
    COMMAND ${CMAKE_COMMAND}
        -DADAPTER_HEADER=${CMAKE_SOURCE_DIR}/src/vita/paf_touchscreen_device_handler.h
        -DOVERLAY_HEADER=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.h
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_touchscreen_diagnostic_probe.cmake
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

add_test(
    NAME livearea_search_release_mode
    COMMAND ${CMAKE_COMMAND}
        -DROOT_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
        -DVITA_CMAKE=${CMAKE_SOURCE_DIR}/cmake/VitaShellPlugin.cmake
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DSEARCH_OVERLAY=${CMAKE_SOURCE_DIR}/src/vita/search_overlay.cpp
        -DSTARTUP_LOG_HEADER=${CMAKE_SOURCE_DIR}/src/vita/startup_log.h
        -DSHELL_STATE_HEADER=${CMAKE_SOURCE_DIR}/src/vita/shell_state.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_release_mode.cmake
)

add_test(
    NAME livearea_search_lsdb_home_bubble
    COMMAND ${CMAKE_COMMAND}
        -DLSDB_ABI=${CMAKE_SOURCE_DIR}/include/livearea_search/lsdb_abi.h
        -DCATALOG_HEADER=${CMAKE_SOURCE_DIR}/include/livearea_search/catalog_record.h
        -DLSDB_HEADER=${CMAKE_SOURCE_DIR}/src/vita/lsdb_index_source.h
        -DLSDB_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/lsdb_index_source.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_lsdb_home_bubble.cmake
)

add_test(
    NAME livearea_search_lsdb_output_ownership
    COMMAND ${CMAKE_COMMAND}
        -DLSDB_ABI=${CMAKE_SOURCE_DIR}/include/livearea_search/lsdb_abi.h
        -DLSDB_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/lsdb_index_source.cpp
        -DOWNERSHIP_REPORT=${CMAKE_SOURCE_DIR}/docs/firmware/filtered-title-array-ownership.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_lsdb_output_ownership.cmake
)

add_test(
    NAME livearea_search_lsdb_query_semantics
    COMMAND ${CMAKE_COMMAND}
        -DLSDB_ABI=${CMAKE_SOURCE_DIR}/include/livearea_search/lsdb_abi.h
        -DLSDB_SOURCE=${CMAKE_SOURCE_DIR}/src/vita/lsdb_index_source.cpp
        -DSEMANTICS_REPORT=${CMAKE_SOURCE_DIR}/docs/firmware/filtered-title-query-semantics.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_lsdb_query_semantics.cmake
)

add_test(
    NAME livearea_search_experimental_self_disable
    COMMAND ${CMAKE_COMMAND}
        -DROOT_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
        -DVITA_CMAKE=${CMAKE_SOURCE_DIR}/cmake/VitaShellPlugin.cmake
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -DBUILD_SCRIPT=${CMAKE_SOURCE_DIR}/tools/build-vita.sh
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_experimental_self_disable.cmake
)

add_test(
    NAME livearea_search_devkit_filebridge_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/devkit/livearea_filebridge/test_bridge.py
        -v
)

add_test(
    NAME livearea_search_devkit_controller_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/devkit/test_livearea_devkit.py
        -v
)

add_test(
    NAME livearea_search_vita_elf_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/test_vita_elf.py
        -v
)

add_test(
    NAME livearea_search_compatibility_audit_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/test_compatibility_audit.py
        -v
)

add_test(
    NAME livearea_search_release_binary_audit_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/test_release_binary_audit.py
        -v
)

add_test(
    NAME livearea_search_binary_audit_integration
    COMMAND ${CMAKE_COMMAND}
        -DROOT_CMAKE=${CMAKE_SOURCE_DIR}/CMakeLists.txt
        -DBUILD_SCRIPT=${CMAKE_SOURCE_DIR}/tools/build-vita.sh
        -DAUDIT_SOURCE=${CMAKE_SOURCE_DIR}/tools/firmware/release_binary_audit.py
        -DCONTRACT_FILE=${CMAKE_SOURCE_DIR}/tools/firmware/release_binary_contract.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_binary_audit_integration.cmake
)

add_test(
    NAME livearea_search_data_directory_naming
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/check_data_directory_naming.py
        --root ${CMAKE_SOURCE_DIR}
)

add_test(
    NAME livearea_search_release_package_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/test_package_release.py
        -v
)

add_test(
    NAME livearea_search_missing_rco_fail_closed
    COMMAND ${CMAKE_COMMAND}
        -DPLUGIN_MAIN=${CMAKE_SOURCE_DIR}/src/vita/plugin_main.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_missing_rco_fail_closed.cmake
)

add_test(
    NAME livearea_search_resource_compatibility_audit_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/test_resource_compatibility_audit.py
        -v
)

add_test(
    NAME livearea_search_source_contract_audit_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/test_source_contract_audit.py
        -v
)

add_test(
    NAME livearea_search_source_contract_current
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tools/firmware/source_contract_audit.py
        --root ${CMAKE_SOURCE_DIR}
        --contract ${CMAKE_SOURCE_DIR}/tools/firmware/source_firmware_contract.json
        --json-out ${CMAKE_BINARY_DIR}/source-contract-report.json
        --markdown-out ${CMAKE_BINARY_DIR}/source-contract-report.md
)

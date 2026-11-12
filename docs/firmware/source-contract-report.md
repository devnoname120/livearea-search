# LiveArea Search source-to-firmware contract audit

22 checks: 22 passed, 0 failed.

| Check | Source | Result |
|---|---|---|
| appmgr-livearea-launch-interface | `src/vita/launcher.cpp` | PASS |
| appmgr-shell-state-interface | `src/vita/plugin_main.cpp` | PASS |
| filtered-title-output-ownership | `src/vita/lsdb_index_source.cpp` | PASS |
| filtered-title-query-semantics | `include/livearea_search/lsdb_abi.h` | PASS |
| indicator-notification-corner-resource-hashes | `include/livearea_search/overlay_visibility.h` | PASS |
| indicator-popup-return-classification | `src/core/overlay_visibility.cpp` | PASS |
| lsdb-abi-layout | `include/livearea_search/lsdb_abi.h` | PASS |
| lsdb-export-contract | `include/livearea_search/lsdb_abi.h` | PASS |
| notification-panel-preemption | `src/vita/search_overlay.cpp` | PASS |
| paf-draw-order-contract | `src/vita/search_overlay.cpp` | PASS |
| paf-ime-exports-and-layout | `src/vita/paf_text_box_ime.h` | PASS |
| paf-pad-export-and-layout | `src/vita/paf_pad_device_handler.h` | PASS |
| paf-page-order-exports | `src/vita/paf_page_order.h` | PASS |
| paf-touchscreen-handler-abi | `src/vita/paf_touchscreen_device_handler.h` | PASS |
| rejected-lsdb-icon-info-abi | `include/livearea_search/lsdb_abi.h` | PASS |
| rejected-lsdb-icon-info-caller | `src/vita/lsdb_index_source.cpp` | PASS |
| release-binary-cleanliness | `CMakeLists.txt` | PASS |
| results-layout-contract | `resources/livearea_search_plugin.xml` | PASS |
| shell-loading-scene-contract | `include/livearea_search/module_readiness.h` | PASS |
| shell-ps-button-ownership | `src/vita/search_overlay.cpp` | PASS |
| system-overlay-identifiers | `src/vita/search_overlay.cpp` | PASS |
| topmenu-resource-hashes | `include/livearea_search/overlay_visibility.h` | PASS |

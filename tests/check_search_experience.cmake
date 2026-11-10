if(NOT DEFINED RESOURCE_XML OR NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED ICON_CACHE_HEADER OR NOT DEFINED ICON_CACHE_SOURCE)
    message(FATAL_ERROR "RESOURCE_XML, SEARCH_OVERLAY, ICON_CACHE_HEADER, and ICON_CACHE_SOURCE are required")
endif()

file(READ "${RESOURCE_XML}" resource_xml)
file(READ "${SEARCH_OVERLAY}" search_overlay)
file(READ "${ICON_CACHE_HEADER}" icon_cache_header)
file(READ "${ICON_CACHE_SOURCE}" icon_cache_source)

string(FIND "${resource_xml}" "text_result_metadata" metadata_index)
if(NOT metadata_index EQUAL -1)
    message(FATAL_ERROR "Result rows must not render title-ID metadata")
endif()

foreach(required_resource IN ITEMS
    "<layout_hint size=\"960,512\" pos=\"0,-16\" />"
    "id=\"plane_results_ime_surface\""
    "<layout_hint size=\"960,208\" pos=\"0,72\" />"
    "<layout_hint align=\"0,0\" anchor=\"0,0\" size=\"960,448\" pos=\"0,-48\" />"
    "id=\"list_results\""
    "id=\"text_box_search\"")
    string(FIND "${resource_xml}" "${required_resource}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing result-surface contract: ${required_resource}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "RefreshSearchFromText(false);"
    "paf::ui::Handler::CB_GESTURE_TAP"
    "OnSearchBoxTap"
    "ShouldAdoptExternalImeEdit("
    "ime external edit adopted"
    "SetResultsEditingLayout"
    "SetResultsDisplayLayout"
    "Vector<SearchResult> next_results;"
    "results_.swap(next_results);"
    "param.cell_index"
    "result_list_->SetSize(960.0f, 224.0f, 0.0f);"
    "result_list_->SetPos(0.0f, 64.0f, 0.0f);"
    "result_list_->SetViewBox(paf::math::v4(-480.0f, 112.0f, 480.0f, -96.0f));")
    string(FIND "${search_overlay}" "${required_source}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing progressive-search or IME re-entry behavior: ${required_source}")
    endif()
endforeach()

string(FIND "${search_overlay}" "void SearchOverlay::SubmitSearch()" submit_start)
string(FIND "${search_overlay}" "void SearchOverlay::RefreshSearchFromText" refresh_start)
if(submit_start EQUAL -1 OR refresh_start EQUAL -1 OR NOT submit_start LESS refresh_start)
    message(FATAL_ERROR "Could not isolate SubmitSearch for progressive-search validation")
endif()
math(EXPR submit_length "${refresh_start} - ${submit_start}")
string(SUBSTRING "${search_overlay}" ${submit_start} ${submit_length} submit_source)
string(FIND "${submit_source}" "RefreshSearchFromText(true);" forced_submit_refresh_index)
if(NOT forced_submit_refresh_index EQUAL -1)
    message(FATAL_ERROR "Return must not rebuild unchanged progressive-search results")
endif()

string(FIND "${search_overlay}" "result_list_->DeleteCell" delete_cells_index)
string(FIND "${search_overlay}" "results_.swap(next_results);" swap_results_index)
if(delete_cells_index EQUAL -1 OR swap_results_index EQUAL -1 OR
   NOT delete_cells_index LESS swap_results_index)
    message(FATAL_ERROR "Old rows must be stopped before their result vector is replaced")
endif()

string(FIND "${search_overlay}" "CB_TEXT_BOX_STRING_CHANGED" unverified_event_index)
if(NOT unverified_event_index EQUAL -1)
    message(FATAL_ERROR "Do not rely on the unverified TextBox string-change event ID")
endif()

string(FIND "${icon_cache_source}" "kMaxCachedSurfaces" hard_surface_cap_index)
if(NOT hard_surface_cap_index EQUAL -1)
    message(FATAL_ERROR "The native surface pool, not a second arbitrary surface cap, must drive eviction")
endif()

foreach(required_cache_api IN ITEMS
    "void Pin(const String& key);"
    "void Unpin(const String& key);"
    "std::uint64_t last_used;"
    "std::size_t pin_count;")
    string(FIND "${icon_cache_header}" "${required_cache_api}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing pin-aware icon-cache API: ${required_cache_api}")
    endif()
endforeach()

foreach(required_cache_source IN ITEMS
    "GetRefCount() == 1"
    "SelectIconCacheEvictionCandidate"
    "entries_[victim].surface.clear()"
    "bool source_opened = false;"
    "LoadSurface(path, source_opened)"
    "while (source_opened && surface.get() == NULL"
    "evicted_surface = entries_[victim].surface;")
    string(FIND "${icon_cache_source}" "${required_cache_source}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing livearea_nolimits-style surface eviction: ${required_cache_source}")
    endif()
endforeach()

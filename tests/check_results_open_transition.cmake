if(NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "SEARCH_OVERLAY is required")
endif()

file(READ "${SEARCH_OVERLAY}" source)

string(FIND "${source}" "int SearchOverlay::OpenResultsPage()" open_start)
string(FIND "${source}" "int SearchOverlay::BeginImeEdit()" open_end)
if(open_start EQUAL -1 OR open_end EQUAL -1 OR NOT open_start LESS open_end)
    message(FATAL_ERROR "Unable to isolate OpenResultsPage")
endif()

math(EXPR open_length "${open_end} - ${open_start}")
string(SUBSTRING "${source}" ${open_start} ${open_length} open_body)

string(FIND "${open_body}" "open_param.fade = false;" no_fade)
if(no_fade EQUAL -1)
    message(FATAL_ERROR
        "The full opaque result page must appear before the system IME transition")
endif()

foreach(forbidden IN ITEMS
    "open_param.fade = true;"
    "open_param.fade_time_ms")
    string(FIND "${open_body}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "The result page must not expose Home through a page fade: ${forbidden}")
    endif()
endforeach()

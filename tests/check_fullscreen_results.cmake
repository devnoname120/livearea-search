if(NOT DEFINED RESOURCE_XML OR NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "RESOURCE_XML and SEARCH_OVERLAY are required")
endif()

file(READ "${RESOURCE_XML}" resource)
file(READ "${SEARCH_OVERLAY}" overlay)

foreach(required IN ITEMS
    "id=\"plane_results_backdrop\""
    "id=\"plane_results_surface\""
    "size=\"960,512\" pos=\"0,-16\""
    "id=\"plane_results_header\""
    "size=\"960,72\" pos=\"0,-32\""
    "id=\"list_results\""
    "size=\"960,440\" pos=\"0,-52\""
    "id=\"plane_results_ime_surface\""
    "size=\"960,200\" pos=\"0,68\""
    "color=\"0.075,0.09,0.12,1\" id=\"style_results_surface\""
    "color=\"0.075,0.09,0.12,1\" id=\"style_results_ime_surface\""
    "color=\"0.11,0.13,0.17,1\" id=\"style_results_header\"")
    string(FIND "${resource}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing full-screen result geometry: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "size=\"960,544\""
    "size=\"920,208\""
    "size=\"920,440\""
    "size=\"920,70\""
    "color=\"0.075,0.09,0.12,0.96\""
    "color=\"0.11,0.13,0.17,0.98\"")
    string(FIND "${resource}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Inset/translucent result geometry remains: ${forbidden}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "SetCellSizeDefault(0, paf::math::v4(960.0f, 70.0f))"
    "result_list_->SetSize(960.0f, 200.0f, 0.0f);"
    "result_list_->SetPos(0.0f, 68.0f, 0.0f);"
    "result_list_->SetSize(960.0f, 440.0f, 0.0f);"
    "result_list_->SetPos(0.0f, -52.0f, 0.0f);")
    string(FIND "${overlay}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing runtime full-screen geometry: ${required_source}")
    endif()
endforeach()

if(NOT DEFINED RESOURCE_XML OR NOT DEFINED OVERLAY_HEADER OR NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR "RESOURCE_XML, OVERLAY_HEADER, and SEARCH_OVERLAY are required")
endif()

file(READ "${RESOURCE_XML}" resource_xml)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" search_overlay)

string(FIND "${resource_xml}"
    "<corner_button style=\"style_home_search_corner\" id=\"button_home_search\">"
    corner_index)
if(corner_index EQUAL -1)
    message(FATAL_ERROR "Home Search must use the standard lower-right native corner control")
endif()

string(FIND "${resource_xml}"
    "<focus_hint enable=\"0\" />"
    no_focus_hint_index)
if(no_focus_hint_index EQUAL -1)
    message(FATAL_ERROR "The Home Search control must opt out of controller focus")
endif()

string(FIND "${resource_xml}"
    "<texture src=\"texture/search.png\" compress=\"on\" id=\"texture_home_search\" type=\"texture/png\" />"
    search_texture_index)
if(search_texture_index EQUAL -1)
    message(FATAL_ERROR "The native corner control must provide a magnifying-glass texture")
endif()

foreach(required_style IN ITEMS
    "<style_corner_button sound=\"_common_sound_corner_button_menu\" key_focus=\"0\" corner_obj=\"home_search_corner_obj\""
    "texture0=\"_common_texture_corner_button_base\" id=\"home_search_bg_obj\""
    "size=\"64,64,0\" texture0=\"texture_home_search\" id=\"home_search_corner_obj\" pos=\"13,-11,0\""
    "texture0=\"_common_texture_corner_button_highlight\" id=\"home_search_highlight_obj\""
    "texture0=\"_common_texture_corner_button_glow\" blend=\"2\" id=\"home_search_glow_obj\"")
    string(FIND "${resource_xml}" "${required_style}" required_style_index)
    if(required_style_index EQUAL -1)
        message(FATAL_ERROR "Missing native corner-style component: ${required_style}")
    endif()
endforeach()

string(FIND "${overlay_header}" "paf::ui::CornerButton* home_button_;" type_index)
if(type_index EQUAL -1)
    message(FATAL_ERROR "The Home Search member type must match the PAF corner control")
endif()

foreach(required_source IN ITEMS
    "home_button_->DisableEvent(paf::ui::EV_FOCUS);"
    "home_button_->DisableEvent(paf::ui::EV_KEYCODE);"
    "home_button_->DisableEvent(paf::ui::EV_ANALOG);"
    "home_button_->ReleaseFocus();")
    string(FIND "${search_overlay}" "${required_source}" required_index)
    if(required_index EQUAL -1)
        message(FATAL_ERROR "Missing touch-only native control behavior: ${required_source}")
    endif()
endforeach()

string(FIND "${search_overlay}" "SetText(home_button_" home_label_index)
if(NOT home_label_index EQUAL -1)
    message(FATAL_ERROR "The compact Home control must not render a text label")
endif()

string(FIND "${search_overlay}" "paf::ui::Button::OBJ_LABEL" inherited_label_index)
if(NOT inherited_label_index EQUAL -1)
    message(FATAL_ERROR "CornerButton's icon is not the inherited Button label object")
endif()

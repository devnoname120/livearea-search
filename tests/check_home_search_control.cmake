if(NOT DEFINED RESOURCE_XML OR NOT DEFINED OVERLAY_HEADER)
    message(FATAL_ERROR "RESOURCE_XML and OVERLAY_HEADER are required")
endif()

file(READ "${RESOURCE_XML}" resource_xml)
file(READ "${OVERLAY_HEADER}" overlay_header)

string(FIND "${resource_xml}"
    "<button sound=\"_common_sound_button_enter\" style=\"_common_default_style_button\" id=\"button_home_search\">"
    button_index)
if(button_index EQUAL -1)
    message(FATAL_ERROR "Home Search must use a label-bearing native PAF button")
endif()

string(FIND "${resource_xml}"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"180,56\" pos=\"-20,20\" />"
    layout_index)
if(layout_index EQUAL -1)
    message(FATAL_ERROR "Home Search must be anchored to the lower-right safe area")
endif()

string(FIND "${resource_xml}"
    "<corner_button style=\"_common_style_corner_bottom_right\" id=\"button_home_search\""
    corner_index)
if(NOT corner_index EQUAL -1)
    message(FATAL_ERROR "The generic lower-right CornerButton renders the ellipsis glyph")
endif()

string(FIND "${overlay_header}" "paf::ui::Button* home_button_;" type_index)
if(type_index EQUAL -1)
    message(FATAL_ERROR "The Home Search member type must match the PAF button resource")
endif()

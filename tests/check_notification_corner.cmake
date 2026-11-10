if(NOT DEFINED RESOURCE_XML OR
   NOT DEFINED NOTIFICATION_HEADER OR
   NOT DEFINED NOTIFICATION_SOURCE OR
   NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR
        "RESOURCE_XML, NOTIFICATION_HEADER, NOTIFICATION_SOURCE, OVERLAY_HEADER, and SEARCH_OVERLAY are required")
endif()

file(READ "${RESOURCE_XML}" resource_xml)
file(READ "${NOTIFICATION_HEADER}" notification_header)
file(READ "${NOTIFICATION_SOURCE}" notification_source)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" search_overlay)

foreach(required_header IN ITEMS
    "class NotificationCornerGuard"
    "explicit NotificationCornerGuard(paf::Plugin* plugin);"
    "bool Suppress();"
    "bool Restore();"
    "paf::Plugin* plugin_;"
    "paf::ui::Scene* page_;")
    string(FIND "${notification_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing notification-corner guard API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "NotificationCornerGuard::NotificationCornerGuard(paf::Plugin* plugin)"
    "occluder_param.overwrite_draw_priority = 7;"
    "plugin_->PageOpen(\"page_notification_occluder\", occluder_param)"
    "plugin_->PageClose(\"page_notification_occluder\", close_param)"
    "NotificationCornerGuard::~NotificationCornerGuard()"
    "const bool notification_restored = notification_corner_.Restore();"
    "Restore();")
    string(FIND "${notification_source}${search_overlay}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing targeted notification-corner behavior: ${required_source}")
    endif()
endforeach()

foreach(forbidden_source IN ITEMS
    "paf::Plugin::Find(\"indicator_plugin\")"
    "paf::Plugin::Find(\"topmenu_plugin\")"
    "PageRoot("
    "FindChild("
    "kNotificationCornerWidgetHash"
    "kNotificationPageHash")
    string(FIND "${notification_source}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Use only the plugin-owned occluder page; never mutate indicator or topmenu page roots or dynamic children: ${forbidden_source}")
    endif()
endforeach()

foreach(required_resource IN ITEMS
    "<page id=\"page_notification_occluder\">"
    "id=\"plane_notification_bar_occluder\""
    "align=\"2,2\" anchor=\"2,2\" size=\"80,32\""
    "id=\"plane_notification_header_occluder\""
    "align=\"2,2\" anchor=\"2,2\" size=\"80,32\" pos=\"0,-32\""
    "color=\"0,0,0,1\" planeobj=\"notification_bar_occluder_obj\""
    "color=\"0.11,0.13,0.17,1\" planeobj=\"notification_header_occluder_obj\"")
    string(FIND "${resource_xml}" "${required_resource}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "Missing narrow high-priority notification occluder resource: ${required_resource}")
    endif()
endforeach()

string(FIND "${overlay_header}" "NotificationCornerGuard notification_corner_;" guard_member)
if(guard_member EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must own the notification-corner guard")
endif()

string(FIND "${search_overlay}" "notification_corner_(plugin)" guard_initialization)
if(guard_initialization EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must bind the occluder guard to its own PAF plugin")
endif()

string(REGEX MATCHALL "notification_corner_\\.Suppress\\(\\);" suppress_calls "${search_overlay}")
list(LENGTH suppress_calls suppress_count)
if(suppress_count LESS 2)
    message(FATAL_ERROR
        "SearchOverlay must suppress once on page open and reassert suppression while open")
endif()

string(FIND "${search_overlay}"
    "if (results_page_ != NULL) {\n        notification_corner_.Suppress();\n    } else {\n        notification_corner_.Restore();\n    }"
    eventual_restore)
if(eventual_restore EQUAL -1)
    message(FATAL_ERROR
        "SearchOverlay must retry restoration after the results page has closed")
endif()

string(FIND "${search_overlay}"
    "const bool notification_restored = notification_corner_.Restore();"
    restore_call)
if(restore_call EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must restore the notification corner on close")
endif()

string(FIND "${search_overlay}" "results_page_ = plugin_->PageOpen(\"page_search_results\"" page_open)
string(FIND "${search_overlay}" "notification_corner_.Suppress();" first_suppress)
if(page_open EQUAL -1 OR first_suppress EQUAL -1 OR NOT page_open LESS first_suppress)
    message(FATAL_ERROR "Notification suppression must begin only after the results page opens")
endif()

string(FIND "${search_overlay}" "plugin_->PageClose(\"page_search_results\"" page_close)
if(page_close EQUAL -1 OR restore_call EQUAL -1 OR NOT page_close LESS restore_call)
    message(FATAL_ERROR "Notification visibility must be restored after the results page closes")
endif()

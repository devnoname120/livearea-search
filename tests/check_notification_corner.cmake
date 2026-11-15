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
    "NotificationCornerGuard();"
    "bool Suppress();"
    "bool Restore();"
    "paf::ui::Widget* ResolveNativeCorner() const;"
    "bool suppressed_;"
    "bool was_visible_;")
    string(FIND "${notification_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing notification-corner guard API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "NotificationCornerGuard::NotificationCornerGuard()"
    "paf::Plugin::Find(\"indicator_plugin\")"
    "indicator->PageRoot(kIndicatorPageHash)"
    "root->FindChild(kIndicatorNotificationBubbleHash)"
    "if (suppressed_) {\n        return true;\n    }"
    "was_visible_ = native_corner->IsVisible();"
    "native_corner->Hide(paf::common::transition::Type_Reset);"
    "native_corner->Show(paf::common::transition::Type_Reset);"
    "NotificationCornerGuard::~NotificationCornerGuard()"
    "const bool notification_restored = notification_corner_.Restore();"
    "Restore();")
    string(FIND "${notification_source}${search_overlay}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing targeted notification-corner behavior: ${required_source}")
    endif()
endforeach()

foreach(forbidden_source IN ITEMS
    "page_notification_occluder"
    "overwrite_draw_priority = 7;"
    "paf::Plugin* plugin_;"
    "paf::ui::Scene* page_;"
    "paf::ui::Widget* native_corner_;"
    "LIVEAREA_SEARCH_DIAGNOSTICS")
    string(FIND "${notification_header}${notification_source}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Notification suppression must use reacquired native-widget ownership without the legacy occluder or retained PAF pointers: ${forbidden_source}")
    endif()
endforeach()

string(FIND "${resource_xml}"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"0,0\" />"
    natural_results_count)
if(natural_results_count EQUAL -1)
    message(FATAL_ERROR "The Recent/results-count label must use the natural right-edge layout")
endif()

foreach(forbidden_resource IN ITEMS
    "page_notification_occluder"
    "plane_notification_bar_occluder"
    "plane_notification_header_occluder"
    "style_notification_bar_occluder"
    "style_notification_header_occluder"
    "pos=\"-80,0\"")
    string(FIND "${resource_xml}" "${forbidden_resource}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "The notification bubble must be hidden natively without an opaque layout reservation: ${forbidden_resource}")
    endif()
endforeach()

string(FIND "${overlay_header}" "NotificationCornerGuard notification_corner_;" guard_member)
if(guard_member EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must own the notification-corner guard")
endif()

string(FIND "${search_overlay}" "notification_corner_()" guard_initialization)
if(guard_initialization EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must initialize the native notification guard without plugin-owned page state")
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

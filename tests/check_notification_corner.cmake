if(NOT DEFINED NOTIFICATION_HEADER OR
   NOT DEFINED NOTIFICATION_SOURCE OR
   NOT DEFINED OVERLAY_HEADER OR
   NOT DEFINED SEARCH_OVERLAY)
    message(FATAL_ERROR
        "NOTIFICATION_HEADER, NOTIFICATION_SOURCE, OVERLAY_HEADER, and SEARCH_OVERLAY are required")
endif()

file(READ "${NOTIFICATION_HEADER}" notification_header)
file(READ "${NOTIFICATION_SOURCE}" notification_source)
file(READ "${OVERLAY_HEADER}" overlay_header)
file(READ "${SEARCH_OVERLAY}" search_overlay)

foreach(required_header IN ITEMS
    "class NotificationCornerGuard"
    "bool Suppress();"
    "bool Restore();"
    "bool suppression_started_;"
    "bool was_visible_;")
    string(FIND "${notification_header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing notification-corner guard API: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "kNotificationCornerWidgetHash = 0x718CB7FFU"
    "paf::Plugin::Find(\"indicator_plugin\")"
    "GetTableElement(paf::resource::TableType_Page)"
    ".GetFirstChild()"
    "paf::cxml::util::GetIDHash(page, \"id\", &page_id)"
    "indicator->PageRoot(page_id)"
    "root->FindChild(paf::IDParam(kNotificationCornerWidgetHash))"
    "page = page.GetNextSibling();"
    "was_visible_ = widget->IsVisible();"
    "widget->Hide(paf::common::transition::Type_Reset);"
    "widget->Show(paf::common::transition::Type_Reset);"
    "NotificationCornerGuard::~NotificationCornerGuard()"
    "const bool notification_restored = notification_corner_.Restore();"
    "Restore();")
    string(FIND "${notification_source}${search_overlay}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing targeted notification-corner behavior: ${required_source}")
    endif()
endforeach()

string(FIND "${notification_source}" "kIndicatorPageHash" fixed_page_hash)
if(NOT fixed_page_hash EQUAL -1)
    message(FATAL_ERROR
        "The indicator page is runtime-selected on 3.60; do not hard-code a page hash")
endif()

foreach(forbidden_source IN ITEMS
    "PageInactivate"
    "indicator->Hide"
    "indicator->Stop")
    string(FIND "${notification_source}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Notification suppression must not hide or stop the whole indicator plugin: ${forbidden_source}")
    endif()
endforeach()

string(FIND "${overlay_header}" "NotificationCornerGuard notification_corner_;" guard_member)
if(guard_member EQUAL -1)
    message(FATAL_ERROR "SearchOverlay must own the notification-corner guard")
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

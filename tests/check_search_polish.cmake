if(NOT DEFINED RESOURCE_XML OR NOT DEFINED SEARCH_OVERLAY OR
   NOT DEFINED OVERLAY_HEADER OR NOT DEFINED PAF_TEXT_BOX_IME_HEADER OR
   NOT DEFINED PAF_PAD_HANDLER_HEADER OR NOT DEFINED VITA_CMAKE)
    message(FATAL_ERROR
        "RESOURCE_XML, SEARCH_OVERLAY, OVERLAY_HEADER, PAF_TEXT_BOX_IME_HEADER, PAF_PAD_HANDLER_HEADER and VITA_CMAKE are required")
endif()

file(READ "${RESOURCE_XML}" resource)
file(READ "${SEARCH_OVERLAY}" source)
file(READ "${OVERLAY_HEADER}" header)
file(READ "${VITA_CMAKE}" vita_cmake)
if(NOT EXISTS "${PAF_TEXT_BOX_IME_HEADER}")
    message(FATAL_ERROR "Missing TextBox IME ABI adapter header")
endif()
file(READ "${PAF_TEXT_BOX_IME_HEADER}" ime_adapter)
if(NOT EXISTS "${PAF_PAD_HANDLER_HEADER}")
    message(FATAL_ERROR "Missing PAF pad device-handler ABI adapter header")
endif()
file(READ "${PAF_PAD_HANDLER_HEADER}" pad_handler_adapter)

foreach(required_resource IN ITEMS
    "<layout_hint align=\"1,1\" anchor=\"1,1\" size=\"750,58\" pos=\"18,0\" />"
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"-80,0\" />"
    "font_size=\"22\""
    "line_pitch=\"32\""
    "id=\"style_results_count\"")
    string(FIND "${resource}" "${required_resource}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing unclipped results-count geometry: ${required_resource}")
    endif()
endforeach()

foreach(required_header IN ITEMS
    "void HandleHome();"
    "void HandleStartButton();"
    "int ResolveShellUtil();"
    "bool AcquireHomePadLock();"
    "void ReleaseHomePadLock();"
    "void UpdateHomePadClose();"
    "static void HomePadDeviceHandler("
    "static SearchOverlay* home_pad_overlay_;"
    "static paf_pad_device_handler::DeviceHandler previous_home_pad_handler_;"
    "bool home_close_pending_;"
    "bool home_pad_press_pending_;"
    "bool home_ps_owned_;"
    "bool start_button_open_pending_;"
    "bool start_button_owned_;"
    "bool home_ps_lock_held_;"
    "ShellUtilLockFunction shell_util_lock_;"
    "ShellUtilUnlockFunction shell_util_unlock_;"
    "void SetHomeVisible(bool visible, bool immediate);"
    "bool IsBubbleHomeVisible(bool& immediate_hide);")
    string(FIND "${header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing Home-close API/state: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "const int kImeOptionNoAssistance = 0x04;"
    "paf::ui::ImeParam ime_param;"
    "paf_text_box_ime::GetImeParam(search_box_, ime_param)"
    "ime_param.option |= kImeOptionNoAssistance;"
    "paf_text_box_ime::SetImeParam(search_box_, ime_param)"
    "paf::Plugin::Find(\"indicator_plugin\")"
    "paf::Plugin::Find(\"livearea_plugin\")"
    "paf::Plugin::Find(\"ime_plugin\")"
    "paf::IDParam(\"page_event_notification\")"
    "IsHomeEditModeVisible(root, scroll_widget)"
    "IsHomeEditOverlayWidget("
    "IsHomeEditScrollWidget("
    "IsHomeBackCornerWidget("
    "sceClibStrcmp(type_name, \"widget_base\") == 0"
    "sceClibStrcmp(type_name, \"corner_button\") == 0"
    "sceClibStrcmp(scroll_type, \"scroll_view\") == 0"
    "child->GetPos(0, root, origin)"
    "child->GetSize(0)"
    "scroll_widget->GetPos(0, root, origin)"
    "scroll_widget->GetSize(0)"
    "return back_corner_visible &&\n        (edit_overlay_visible || full_screen_scroll);"
    "ShouldExposeHomeSearch("
    "home_button_->DisableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);"
    "home_button_->EnableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);"
    "paf::inputdevice::pad::Data::Keycode::PAD_PS"
    "paf::inputdevice::pad::Data::Keycode::PAD_START"
    "overlay->HandleHome();"
    "overlay->HandleStartButton();"
    "previous_home_pad_handler_ = paf_pad_device_handler::SetDeviceHandler(HomePadDeviceHandler);"
    "paf_pad_device_handler::SetDeviceHandler(previous_home_pad_handler_)"
    "paf::inputdevice::pad::Data forwarded;"
    "forwarded = *data;"
    "forwarded_data = &forwarded;"
    "ObserveHomePadPress("
    "ObserveStartButtonPress("
    "overlay->results_page_ != NULL,"
    "overlay->home_visible_ && overlay->results_page_ == NULL,"
    "forwarded.paddata,"
    "previous_home_pad_handler_(forwarded_data);"
    "home_pad_press_pending_ = true;"
    "start_button_open_pending_ = true;"
    "start pad device callback"
    "void SearchOverlay::HandleStartButton()"
    "if (!home_visible_ || results_page_ != NULL)"
    "if (!IsBubbleHomeVisible(immediate_hide))"
    "start open results page"
    "search_box_->EndEdit()"
    "home_close_pending_ = true;"
    "results_pending_display_ = false;"
    "const HomePadCloseAction action = ComputeHomePadCloseAction("
    "HomePadCloseAction::ReleaseLockAndClose"
    "ReleaseHomePadLock();"
    "RequestCloseResultsPage();"
    "if (!AcquireHomePadLock())"
    "kShellUtilLockNid = 0xA9537233U;"
    "kShellUtilUnlockNid = 0x21A6CF54U;"
    "shell_util_lock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2)"
    "shell_util_unlock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2)"
    "home_page_->Show(paf::common::transition::Type_FadeinFast);"
    "HomeVisibilityAction::HideImmediately"
    "home_page_->Hide(\n            immediate ?\n                paf::common::transition::Type_Reset :\n                paf::common::transition::Type_FadeinFast);")
    string(FIND "${source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing IME/Home polish behavior: ${required_source}")
    endif()
endforeach()

foreach(forbidden_source IN ITEMS
    "paf::Plugin::Find(\"impose_plugin\")"
    "paf::Plugin::Find(\"impose_net_plugin\")"
    "shell_util_lock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "shell_util_unlock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "\"edit_decide\""
    "\"edit_open\""
    "\"edit_cancel\""
    "\"edit_back\""
    "\"edit_modeMode\""
    "\"edit_menu\""
    "\"edit_kakutei\""
    "LogTopMenuTree("
    "QueueVisibleWidgetTree("
    "HashWidgetTree("
    "topmenu tree")
    string(FIND "${source}" "${forbidden_source}" found)
    if(NOT found EQUAL -1)
        if(forbidden_source MATCHES "SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN")
            message(FATAL_ERROR
                "Search must use the no-stop-symbol PS_BTN_2 shell lock: ${forbidden_source}")
        else()
            message(FATAL_ERROR
                "Search must use the AppMgr system-UI overlay word instead of a coarse plugin scan: ${forbidden_source}")
        endif()
    endif()
endforeach()

string(FIND "${source}"
    "event_notification_visible,\n        livearea_visible,\n        external_ime_visible,\n        home_edit_mode_visible);"
    exclusive_home_gate)
if(exclusive_home_gate EQUAL -1)
    message(FATAL_ERROR
        "Search visibility must pass LiveArea, IME, and edit ownership into the exclusive Home gate")
endif()

string(FIND "${source}" "int SearchOverlay::OpenHomePage()" open_home_start)
string(FIND "${source}"
    "home_button_->DisableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);"
    initial_touch_disable)
string(FIND "${source}"
    "home_page_->Hide(paf::common::transition::Type_Reset);"
    initial_page_hide)
string(FIND "${source}"
    "plugin_->PageInactivate(\"page_home_search\");"
    initial_page_inactivate)
string(FIND "${source}" "UpdateVisibility();" initial_visibility_update)
if(open_home_start EQUAL -1 OR initial_touch_disable EQUAL -1 OR
   initial_page_hide EQUAL -1 OR initial_page_inactivate EQUAL -1 OR
   initial_visibility_update EQUAL -1 OR
   NOT open_home_start LESS initial_touch_disable OR
   NOT initial_touch_disable LESS initial_page_hide OR
   NOT initial_page_hide LESS initial_page_inactivate OR
   NOT initial_page_inactivate LESS initial_visibility_update)
    message(FATAL_ERROR
        "The Home search page must start hidden and non-interactive before its first visibility evaluation")
endif()

string(FIND "${source}" "overlay != NULL && overlay->IsBubbleHomeVisible(immediate_hide)" activation_gate)
string(FIND "${source}" "overlay->OpenResultsPage()" activation_open)
if(activation_gate EQUAL -1 OR activation_open EQUAL -1 OR
   NOT activation_gate LESS activation_open)
    message(FATAL_ERROR "Search activation must synchronously reject impose/system overlays")
endif()

foreach(required_adapter IN ITEMS
    "_ZN3paf11inputdevice3pad16SetDeviceHandlerEPFvPNS1_4DataEE"
    "typedef void(*DeviceHandler)(paf::inputdevice::pad::Data*)"
    "inline DeviceHandler SetDeviceHandler("
    "static_assert(sizeof(paf::inputdevice::pad::Data) == 20")
    string(FIND "${pad_handler_adapter}" "${required_adapter}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing PAF pad device-handler ABI adapter: ${required_adapter}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
    "FilterHomePadPress("
    "ReleaseHomePadOwnershipIfUp("
    "sceCtrlPeekBufferPositive"
    "sceCtrlPeekBufferPositiveExt"
    "data->paddata ="
    "OnResultsKeyPress"
    "home_page_->Show(paf::common::transition::Type_Reset);"
    "SceShellSvc_stub"
    "SceCtrl_stub")
    string(FIND "${source}${vita_cmake}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Home ownership must filter only the forwarded PAF copy: ${forbidden}")
    endif()
endforeach()

string(FIND "${source}" "ObserveHomePadPress(" filter_index)
string(FIND "${source}" "ObserveStartButtonPress(" start_filter_index)
string(FIND "${source}" "previous_home_pad_handler_(forwarded_data);" forward_index)
if(filter_index EQUAL -1 OR start_filter_index EQUAL -1 OR forward_index EQUAL -1 OR
   NOT filter_index LESS forward_index OR NOT start_filter_index LESS forward_index)
    message(FATAL_ERROR "Owned PS and Start bits must be removed before forwarding the PAF sample")
endif()

string(FIND "${source}" "void SearchOverlay::HandleStartButton()" start_handler_index)
string(FIND "${source}" "if (!home_visible_ || results_page_ != NULL)" start_visible_gate_index)
string(FIND "${source}" "if (!IsBubbleHomeVisible(immediate_hide))" start_live_gate_index)
string(FIND "${source}" "QueueInteractionLogResult(\"start open results page\", OpenResultsPage());" start_open_index)
if(start_handler_index EQUAL -1 OR start_visible_gate_index EQUAL -1 OR
   start_live_gate_index EQUAL -1 OR start_open_index EQUAL -1 OR
   NOT start_handler_index LESS start_visible_gate_index OR
   NOT start_visible_gate_index LESS start_live_gate_index OR
   NOT start_live_gate_index LESS start_open_index)
    message(FATAL_ERROR "Start must open Search only after both visible and live Home gates pass")
endif()

string(FIND "${source}" "int SearchOverlay::OpenResultsPage()" open_results_start)
string(FIND "${source}" "if (!AcquireHomePadLock())" acquire_lock_index)
string(FIND "${source}" "results_page_ = plugin_->PageOpen" page_open_index)
if(open_results_start EQUAL -1 OR acquire_lock_index EQUAL -1 OR page_open_index EQUAL -1 OR
   NOT open_results_start LESS acquire_lock_index OR NOT acquire_lock_index LESS page_open_index)
    message(FATAL_ERROR "Search must acquire SceShell PS ownership before opening its results page")
endif()

string(FIND "${source}" "void SearchOverlay::HandleHome()" handle_home_start)
string(FIND "${source}" "void SearchOverlay::UpdateHomePadClose()" update_home_start)
if(handle_home_start EQUAL -1 OR update_home_start EQUAL -1 OR
   NOT handle_home_start LESS update_home_start)
    message(FATAL_ERROR "Missing bounded Home-close implementation")
endif()
math(EXPR handle_home_length "${update_home_start} - ${handle_home_start}")
string(SUBSTRING "${source}" ${handle_home_start} ${handle_home_length} handle_home_source)
string(FIND "${handle_home_source}" "RequestCloseResultsPage();" immediate_home_close)
if(NOT immediate_home_close EQUAL -1)
    message(FATAL_ERROR "Search must remain visible until the owned PS press is physically released")
endif()

string(FIND "${source}" "void SearchOverlay::RequestCloseResultsPage()" request_close_start)
if(update_home_start EQUAL -1 OR request_close_start EQUAL -1 OR
   NOT update_home_start LESS request_close_start)
    message(FATAL_ERROR "Missing delayed Home-close policy body")
endif()
math(EXPR update_home_length "${request_close_start} - ${update_home_start}")
string(SUBSTRING "${source}" ${update_home_start} ${update_home_length} update_home_source)
string(FIND "${update_home_source}" "const HomePadCloseAction action = ComputeHomePadCloseAction(" close_policy_index)
string(FIND "${update_home_source}" "ReleaseHomePadLock();" unlock_index)
string(FIND "${update_home_source}" "RequestCloseResultsPage();" delayed_close_index)
if(close_policy_index EQUAL -1 OR unlock_index EQUAL -1 OR delayed_close_index EQUAL -1 OR
   NOT close_policy_index LESS unlock_index OR NOT unlock_index LESS delayed_close_index)
    message(FATAL_ERROR "Button-up must unlock SceShell before Search exposes Home")
endif()

foreach(forbidden_listener IN ITEMS
    "class HomePadListener"
    "AddInputListener"
    "DelInputListener"
    "home_pad_listener_")
    string(FIND "${source}${header}" "${forbidden_listener}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Home interception must occur before PAF listener dispatch: ${forbidden_listener}")
    endif()
endforeach()

foreach(required_adapter IN ITEMS
    "_ZN3paf2ui7TextBox11GetImeParamERNS1_8ImeParamE"
    "_ZN3paf2ui7TextBox11SetImeParamERKNS1_8ImeParamE"
    "inline int GetImeParam("
    "inline int SetImeParam("
    "static_assert(sizeof(paf::ui::ImeParam) == 24")
    string(FIND "${ime_adapter}" "${required_adapter}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing TextBox IME ABI adapter: ${required_adapter}")
    endif()
endforeach()

string(FIND "${source}" "paf_text_box_ime::GetImeParam(search_box_, ime_param)" get_ime_index)
string(FIND "${source}" "paf_text_box_ime::SetImeParam(search_box_, ime_param)" set_ime_index)
string(FIND "${source}" "const int result = search_box_->BeginEdit();" begin_edit_index)
if(get_ime_index EQUAL -1 OR set_ime_index EQUAL -1 OR begin_edit_index EQUAL -1 OR
   NOT get_ime_index LESS set_ime_index OR NOT set_ime_index LESS begin_edit_index)
    message(FATAL_ERROR "IME no-assistance option must be read, preserved and written before BeginEdit")
endif()

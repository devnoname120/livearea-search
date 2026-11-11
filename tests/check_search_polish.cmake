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
    "<layout_hint align=\"2,1\" anchor=\"2,1\" size=\"150,64\" pos=\"-18,0\" />"
    "font_size=\"22\""
    "line_pitch=\"32\""
    "id=\"style_results_count\"")
    string(FIND "${resource}" "${required_resource}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing unclipped results-count geometry: ${required_resource}")
    endif()
endforeach()

foreach(required_header IN ITEMS
    "typedef int (*ShellUtilInitEventsFunction)(int);"
    "typedef int (*ShellUtilLockFunction)(SceShellUtilLockType);"
    "typedef int (*ShellUtilUnlockFunction)(SceShellUtilLockType);"
    "int ResolveShellUtil();"
    "void HandleHome();"
    "bool AcquireHomePadLock();"
    "void ReleaseHomePadLock();"
    "void UpdateHomePadLock();"
    "static void OnResultsKeyPress("
    "static void HomePadDeviceHandler("
    "static SearchOverlay* home_pad_overlay_;"
    "static paf_pad_device_handler::DeviceHandler previous_home_pad_handler_;"
    "bool home_close_pending_;"
    "bool home_pad_press_pending_;"
    "bool home_ps_owned_;"
    "bool shell_util_ready_;"
    "bool home_ps_lock_held_;"
    "ShellUtilInitEventsFunction shell_util_init_events_;"
    "ShellUtilLockFunction shell_util_lock_;"
    "ShellUtilUnlockFunction shell_util_unlock_;")
    string(FIND "${header}" "${required_header}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing Home-close API/state: ${required_header}")
    endif()
endforeach()

foreach(required_source IN ITEMS
    "const char* kShellSvcModuleName = \"SceShellSvc\";"
    "const std::uint32_t kShellUtilLibraryNid = 0xD2B1C8AEU;"
    "const std::uint32_t kShellUtilInitEventsNid = 0xE8AD11ECU;"
    "const std::uint32_t kShellUtilLockNid = 0xA9537233U;"
    "const std::uint32_t kShellUtilUnlockNid = 0x21A6CF54U;"
    "taiGetModuleExportFunc("
    "int SearchOverlay::ResolveShellUtil()"
    "ResolveShellUtilFunction(kShellUtilInitEventsNid, shell_util_init_events_)"
    "ResolveShellUtilFunction(kShellUtilLockNid, shell_util_lock_)"
    "ResolveShellUtilFunction(kShellUtilUnlockNid, shell_util_unlock_)"
    "const int kImeOptionNoAssistance = 0x04;"
    "paf::ui::ImeParam ime_param;"
    "paf_text_box_ime::GetImeParam(search_box_, ime_param)"
    "ime_param.option |= kImeOptionNoAssistance;"
    "paf_text_box_ime::SetImeParam(search_box_, ime_param)"
    "paf::inputdevice::pad::Data::Keycode::PAD_PS"
    "OnResultsKeyPress"
    "event->Consume();"
    "overlay->HandleHome();"
    "previous_home_pad_handler_ = paf_pad_device_handler::SetDeviceHandler(HomePadDeviceHandler);"
    "paf_pad_device_handler::SetDeviceHandler(previous_home_pad_handler_)"
    "shell_util_init_events_(0)"
    "shell_util_lock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "shell_util_unlock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "ObserveHomePadPress("
    "overlay->results_page_ != NULL,"
    "data->paddata,"
    "ShouldReleaseHomePadLock("
    "overlay->UpdateHomePadLock();"
    "home_pad_press_pending_ = true;"
    "search_box_->EndEdit()"
    "home_close_pending_ = true;"
    "results_pending_display_ = false;"
    "RequestCloseResultsPage();"
    "home_page_->Show(paf::common::transition::Type_FadeinFast);")
    string(FIND "${source}" "${required_source}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing IME/Home polish behavior: ${required_source}")
    endif()
endforeach()

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
    "data->paddata = current_buttons;"
    "sceShellUtilInitEvents(0)"
    "sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN)"
    "home_page_->Show(paf::common::transition::Type_Reset);"
    "SceShellSvc_stub"
    "SceCtrl_stub")
    string(FIND "${source}${vita_cmake}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Home ownership must use an unmodified PAF sample and the shell lock: ${forbidden}")
    endif()
endforeach()

string(FIND "${source}" "if (!AcquireHomePadLock())" acquire_lock_index)
string(FIND "${source}" "plugin_->PageOpen(\"page_search_results\"" open_page_index)
if(acquire_lock_index EQUAL -1 OR open_page_index EQUAL -1 OR
   NOT acquire_lock_index LESS open_page_index)
    message(FATAL_ERROR "The shell PS lock must be acquired before Search becomes interactive")
endif()

string(REGEX MATCHALL "ReleaseHomePadLock\\(\\);" release_lock_calls "${source}")
list(LENGTH release_lock_calls release_lock_call_count)
if(release_lock_call_count LESS 2)
    message(FATAL_ERROR "The shell PS lock must be released on normal close and teardown")
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

string(FIND "${source}" "event->Consume();" consume_index)
string(FIND "${source}" "overlay->HandleHome();" home_index)
if(consume_index EQUAL -1 OR home_index EQUAL -1 OR NOT consume_index LESS home_index)
    message(FATAL_ERROR "The PS event must be consumed before Search begins closing")
endif()

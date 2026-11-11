#include "search_overlay.h"

#include <psp2/kernel/clib.h>
#include <taihen.h>

#include <livearea_search/overlay_visibility.h>

#include "launcher.h"
#include "paf_memory.h"
#include "paf_pad_device_handler.h"
#include "paf_page_order.h"
#include "paf_text_box_ime.h"
#include "shell_state.h"
#include "startup_log.h"

namespace livearea_search {
namespace {

const int kResultsDisplayDrawPriority = 6;
const int kResultsEditingDrawPriority = 36;
const int kImeOptionNoAssistance = 0x04;
const int kShellUtilAlreadyInitialized = static_cast<int>(0x80101802U);
const char* kShellSvcModuleName = "SceShellSvc";
const std::uint32_t kShellUtilLibraryNid = 0xD2B1C8AEU;
const std::uint32_t kShellUtilInitEventsNid = 0xE8AD11ECU;
const std::uint32_t kShellUtilLockNid = 0xA9537233U;
const std::uint32_t kShellUtilUnlockNid = 0x21A6CF54U;

template <typename Function>
int ResolveShellUtilFunction(std::uint32_t nid, Function& function) {
    std::uintptr_t address = 0;
    const int result = taiGetModuleExportFunc(
        kShellSvcModuleName,
        kShellUtilLibraryNid,
        nid,
        &address);
    if (result < 0) {
        return result;
    }
    if (address == 0) {
        return -1;
    }
    function = reinterpret_cast<Function>(address);
    return 0;
}

} // namespace

SearchOverlay* SearchOverlay::home_pad_overlay_ = NULL;
paf_pad_device_handler::DeviceHandler SearchOverlay::previous_home_pad_handler_ = NULL;

SearchOverlay::ResultItemFactory::ResultItemFactory(SearchOverlay* overlay)
    : overlay_(overlay) {}

paf::ui::ListItem* SearchOverlay::ResultItemFactory::Create(CreateParam& param) {
    if (overlay_ == NULL || param.parent == NULL || param.cell_index < 0) {
        return NULL;
    }

    const SearchResult* result = overlay_->ResultAt(static_cast<std::size_t>(param.cell_index));
    if (result == NULL || result->app == NULL) {
        return NULL;
    }

    paf::Plugin::TemplateOpenParam open_param;
    overlay_->plugin_->TemplateOpen(param.parent, "template_result_item", open_param);
    if (param.parent->GetChildrenNum() <= 0) {
        return NULL;
    }

    paf::ui::ListItem* item = static_cast<paf::ui::ListItem*>(
        param.parent->GetChild(param.parent->GetChildrenNum() - 1));
    paf::ui::Widget* button = item->FindChild("button_result");
    paf::ui::Widget* title = item->FindChild("text_result_title");
    if (button == NULL || title == NULL) {
        return item;
    }

    const AppRecord& app = *result->app;
    button->SetName(app.title_id.c_str());
    SearchOverlay::SetText(title, app.display_title);

    button->AddEventCallback(
        paf::ui::Button::CB_BTN_DECIDE,
        SearchOverlay::OnResultDecide,
        overlay_);
    button->AddEventCallback(
        IconCache::kReadyEvent,
        SearchOverlay::OnIconReady,
        overlay_);
    overlay_->ApplyIcon(button, app.title_id);
    return item;
}

void SearchOverlay::ResultItemFactory::Start(StartParam& param) {
    if (param.list_item != NULL) {
        const SearchResult* result =
            overlay_ == NULL || param.cell_index < 0 ? NULL :
            overlay_->ResultAt(static_cast<std::size_t>(param.cell_index));
        paf::ui::Widget* button = param.list_item->FindChild("button_result");
        if (result != NULL && result->app != NULL && button != NULL &&
            overlay_->icon_cache_ != NULL) {
            overlay_->icon_cache_->Pin(result->app->title_id);
            overlay_->ApplyIcon(button, result->app->title_id);
        }
        param.list_item->Show(paf::common::transition::Type_FadeinFast);
    }
}

void SearchOverlay::ResultItemFactory::Stop(StopParam& param) {
    if (param.list_item != NULL) {
        const SearchResult* result =
            overlay_ == NULL || param.cell_index < 0 ? NULL :
            overlay_->ResultAt(static_cast<std::size_t>(param.cell_index));
        if (result != NULL && result->app != NULL && overlay_->icon_cache_ != NULL) {
            overlay_->icon_cache_->Unpin(result->app->title_id);
        }
        param.list_item->Hide(paf::common::transition::Type_FadeinFast);
    }
}

SearchOverlay::SearchOverlay(paf::Plugin* plugin)
    : plugin_(plugin),
      home_page_(NULL),
      results_page_(NULL),
      home_button_(NULL),
      search_box_(NULL),
      results_backdrop_(NULL),
      results_surface_(NULL),
      results_ime_surface_(NULL),
      result_list_(NULL),
      empty_text_(NULL),
      result_count_(NULL),
      close_button_(NULL),
      notification_corner_(plugin),
      icon_cache_(NULL),
      visibility_registered_(false),
      home_visible_(false),
      ime_transition_active_(false),
      ime_editing_seen_(false),
      results_pending_display_(false),
      ime_close_pending_(false),
      results_editing_layout_(false),
      results_draw_priority_(kResultsDisplayDrawPriority),
      close_requested_(false),
      close_defer_frames_(0),
      home_close_pending_(false),
      home_pad_press_pending_(false),
      home_ps_owned_(false),
      shell_util_ready_(false),
      home_ps_lock_held_(false),
      shell_util_init_events_(NULL),
      shell_util_lock_(NULL),
      shell_util_unlock_(NULL),
      initialized_(false) {}

SearchOverlay::~SearchOverlay() {
    visibility_registered_ = false;
    paf::common::MainThreadCallList::Unregister(VisibilityTick, this);
    if (home_pad_overlay_ == this) {
        home_pad_overlay_ = NULL;
        const paf_pad_device_handler::DeviceHandler displaced =
            paf_pad_device_handler::SetDeviceHandler(previous_home_pad_handler_);
        if (displaced != HomePadDeviceHandler) {
            paf_pad_device_handler::SetDeviceHandler(displaced);
        }
        previous_home_pad_handler_ = NULL;
    }
    CloseResultsPage();
    ReleaseHomePadLock();

    if (plugin_ != NULL && home_page_ != NULL) {
        paf::Plugin::PageCloseParam close_param;
        close_param.fade = false;
        plugin_->PageClose("page_home_search", close_param);
    }
    home_page_ = NULL;
    home_button_ = NULL;
    home_visible_ = false;

    delete icon_cache_;
    icon_cache_ = NULL;
}

int SearchOverlay::Initialize(AppIndex& prepared_index, RecentApps& prepared_recent_apps) {
    StartupLog("overlay initialize begin");
    if (initialized_) {
        return 0;
    }
    if (plugin_ == NULL) {
        return -1;
    }

    int result = ResolveShellUtil();
    StartupLogResult("shell util resolve", result);
    if (result < 0) {
        return result;
    }

    result = shell_util_init_events_(0);
    StartupLogResult("shell util init events", result);
    shell_util_ready_ = result >= 0 || result == kShellUtilAlreadyInitialized;
    if (!shell_util_ready_) {
        return result;
    }
    index_.Swap(prepared_index);
    recent_apps_.Swap(prepared_recent_apps);

    icon_cache_ = PafNew<IconCache>(plugin_);
    StartupLogResult(
        "overlay icon cache ready",
        icon_cache_ != NULL && icon_cache_->ready() ? 1 : 0);
    if (icon_cache_ == NULL || !icon_cache_->ready()) {
        delete icon_cache_;
        icon_cache_ = NULL;
        return -1;
    }
    result = OpenHomePage();
    StartupLogResult("overlay open home page", result);
    if (result < 0) {
        delete icon_cache_;
        icon_cache_ = NULL;
        return result;
    }
    if (home_pad_overlay_ != NULL) {
        return -1;
    }
    home_pad_overlay_ = this;
    previous_home_pad_handler_ = paf_pad_device_handler::SetDeviceHandler(HomePadDeviceHandler);
    StartupLogResult(
        "overlay previous home pad handler",
        previous_home_pad_handler_ != NULL ? 1 : 0);

    initialized_ = true;
    visibility_registered_ = true;
    paf::common::MainThreadCallList::Register(VisibilityTick, this);
    return 0;
}

int SearchOverlay::OpenHomePage() {
    StartupLog("home page open begin");
    paf::Plugin::PageOpenParam open_param;
    open_param.fade = false;
    open_param.overwrite_draw_priority = 6;
    home_page_ = plugin_->PageOpen("page_home_search", open_param);
    if (home_page_ == NULL) {
        StartupLog("home page PageOpen returned null");
        return -1;
    }

    home_button_ = static_cast<paf::ui::CornerButton*>(
        home_page_->FindChild("button_home_search"));
    if (home_button_ == NULL) {
        StartupLog("home page search button missing");
        return -1;
    }
    home_button_->AddEventCallback(
        paf::ui::Button::CB_BTN_DECIDE,
        OnHomeSearch,
        this);
    home_button_->DisableEvent(paf::ui::EV_FOCUS);
    home_button_->DisableEvent(paf::ui::EV_KEYCODE);
    home_button_->DisableEvent(paf::ui::EV_ANALOG);
    home_button_->ReleaseFocus();
    StartupLogResult("home shell allows overlay", ShellAllowsOverlay() ? 1 : 0);
    paf::Plugin* top_menu = paf::Plugin::Find("topmenu_plugin");
    StartupLogResult("home topmenu plugin found", top_menu != NULL ? 1 : 0);
    StartupLogResult("home topmenu plugin visible", IsPafPluginVisible(top_menu) ? 1 : 0);
    UpdateVisibility();
    StartupLogResult("home visible after update", home_visible_ ? 1 : 0);
    return 0;
}

int SearchOverlay::OpenResultsPage() {
    if (results_page_ != NULL) {
        return BeginImeEdit();
    }

    close_requested_ = false;
    close_defer_frames_ = 0;
    home_close_pending_ = false;
    home_pad_press_pending_ = false;
    if (!AcquireHomePadLock()) {
        return -1;
    }

    paf::Plugin::PageOpenParam open_param;
    open_param.fade = false;
    results_page_ = plugin_->PageOpen("page_search_results", open_param);
    if (results_page_ == NULL) {
        ReleaseHomePadLock();
        return -1;
    }
    QueueInteractionLogResult(
        "notification corner suppress",
        notification_corner_.Suppress() ? 1 : 0);

    search_box_ = static_cast<paf::ui::TextBox*>(results_page_->FindChild("text_box_search"));
    results_backdrop_ = results_page_->FindChild("plane_results_backdrop");
    results_surface_ = results_page_->FindChild("plane_results_surface");
    results_ime_surface_ = results_page_->FindChild("plane_results_ime_surface");
    result_list_ = static_cast<paf::ui::ListView*>(results_page_->FindChild("list_results"));
    empty_text_ = static_cast<paf::ui::Text*>(results_page_->FindChild("text_empty"));
    result_count_ = static_cast<paf::ui::Text*>(results_page_->FindChild("text_results_count"));
    close_button_ = static_cast<paf::ui::CornerButton*>(
        results_page_->FindChild("button_results_close"));
    if (search_box_ == NULL || results_backdrop_ == NULL || results_surface_ == NULL ||
        results_ime_surface_ == NULL || result_list_ == NULL || empty_text_ == NULL ||
        result_count_ == NULL || close_button_ == NULL) {
        CloseResultsPage();
        return -1;
    }

    results_page_->SetKeycode(
        paf::inputdevice::pad::Data::Keycode::PAD_ESCAPE |
        paf::inputdevice::pad::Data::Keycode::PAD_PS);
    results_page_->AddEventCallback(
        paf::ui::Handler::CB_KEYCODE_PRESS,
        OnBackKey,
        this);
    results_page_->AddEventCallback(
        paf::ui::Handler::CB_KEYCODE_PRESS,
        OnResultsKeyPress,
        this);

    search_box_->SetMaxLength(64);
    search_box_->AddEventCallback(
        paf::ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
        OnSearchSubmit,
        this);
    search_box_->AddEventCallback(
        paf::ui::Handler::CB_GESTURE_TAP,
        OnSearchBoxTap,
        this);
    close_button_->AddEventCallback(
        paf::ui::Button::CB_BTN_DECIDE,
        OnClose,
        this);
    SetText(close_button_, "Close");
    SetText(empty_text_, "No matching applications");

    ResultItemFactory* factory = PafNew<ResultItemFactory>(this);
    if (factory == NULL) {
        CloseResultsPage();
        return -1;
    }
    result_list_->SetItemFactory(factory);
    result_list_->InsertSegment(0, 1);
    result_list_->SetCellSizeDefault(0, paf::math::v4(960.0f, 70.0f));
    result_list_->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST);
    result_list_->SetScrollType(paf::ui::ListView::SCROLL_TYPE_VERTICAL);

    results_.clear();
    normalized_query_.clear();
    empty_text_->Hide(paf::common::transition::Type_Reset);
    SetResultsEditingLayout();
    UpdateVisibility();
    if (results_page_ == NULL || search_box_ == NULL) {
        return -1;
    }
    icon_cache_->SetNotificationsEnabled(true);
    RefreshSearchFromText(true);
    return BeginImeEdit();
}

int SearchOverlay::BeginImeEdit() {
    if (search_box_ == NULL) {
        return -1;
    }

    SetResultsEditingLayout();
    SetResultsDrawPriority(kResultsEditingDrawPriority);
    ime_transition_active_ = true;
    ime_editing_seen_ = false;
    results_pending_display_ = true;
    ime_close_pending_ = false;
    paf::ui::ImeParam ime_param;
    const int get_ime_result = paf_text_box_ime::GetImeParam(search_box_, ime_param);
    QueueInteractionLogResult("ime get param", get_ime_result);
    if (get_ime_result >= 0) {
        ime_param.option |= kImeOptionNoAssistance;
        QueueInteractionLogResult(
            "ime set param",
            paf_text_box_ime::SetImeParam(search_box_, ime_param));
    }
    const int result = search_box_->BeginEdit();
    QueueInteractionLogResult("ime begin edit", result);
    if (result < 0) {
        ime_transition_active_ = false;
        RequestCloseResultsPage();
    }
    return result;
}

int SearchOverlay::ResolveShellUtil() {
    shell_util_init_events_ = NULL;
    shell_util_lock_ = NULL;
    shell_util_unlock_ = NULL;

    int result = ResolveShellUtilFunction(kShellUtilInitEventsNid, shell_util_init_events_);
    if (result < 0) {
        return result;
    }
    result = ResolveShellUtilFunction(kShellUtilLockNid, shell_util_lock_);
    if (result < 0) {
        return result;
    }
    result = ResolveShellUtilFunction(kShellUtilUnlockNid, shell_util_unlock_);
    if (result < 0) {
        return result;
    }
    return 0;
}

void SearchOverlay::HandleBack() {
    const ResultsBackAction action = ComputeResultsBackAction(
        results_page_ != NULL,
        search_box_ != NULL && search_box_->IsEditing(),
        ime_transition_active_);
    if (action == ResultsBackAction::EndImeEdit) {
        QueueInteractionLog("back dismiss ime");
        ime_close_pending_ = false;
        results_pending_display_ = true;
        if (search_box_ != NULL) {
            QueueInteractionLogResult("back end edit", search_box_->EndEdit());
        }
        return;
    }
    if (action == ResultsBackAction::CloseResults) {
        QueueInteractionLog("back close results");
        RequestCloseResultsPage();
    }
}

void SearchOverlay::HandleHome() {
    if (results_page_ == NULL || home_close_pending_) {
        return;
    }
    home_close_pending_ = true;
    results_pending_display_ = false;
    ime_close_pending_ = true;
    ime_transition_active_ = false;
    ime_editing_seen_ = false;
    if (search_box_ != NULL) {
        QueueInteractionLogResult("home end edit", search_box_->EndEdit());
    }
    RequestCloseResultsPage();
}

bool SearchOverlay::AcquireHomePadLock() {
    if (home_ps_lock_held_) {
        return true;
    }
    if (!shell_util_ready_ || shell_util_lock_ == NULL) {
        return false;
    }
    const int result = shell_util_lock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
    QueueInteractionLogResult("home PS lock", result);
    if (result < 0) {
        return false;
    }
    home_ps_lock_held_ = true;
    return true;
}

void SearchOverlay::ReleaseHomePadLock() {
    if (!home_ps_lock_held_ || shell_util_unlock_ == NULL) {
        return;
    }
    const int result = shell_util_unlock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);
    QueueInteractionLogResult("home PS unlock", result);
    if (result >= 0) {
        home_ps_lock_held_ = false;
    }
}

void SearchOverlay::UpdateHomePadLock() {
    if (ShouldReleaseHomePadLock(
            home_ps_lock_held_,
            results_page_ != NULL,
            home_ps_owned_)) {
        ReleaseHomePadLock();
    }
}

void SearchOverlay::RequestCloseResultsPage() {
    if (results_page_ == NULL || close_requested_) {
        return;
    }
    close_requested_ = true;
    close_defer_frames_ = 1;
    if (result_list_ != NULL) {
        result_list_->DisableEvent(
            paf::ui::EV_TOUCH |
            paf::ui::EV_GESTURE_ALL |
            paf::ui::EV_KEYCODE |
            paf::ui::EV_ANALOG |
            paf::ui::EV_FOCUS);
        result_list_->ReleaseFocus();
    }
}

void SearchOverlay::ProcessCloseRequest() {
    const DeferredResultCloseAction action = AdvanceDeferredResultClose(
        close_requested_,
        close_defer_frames_);
    if (action != DeferredResultCloseAction::Close) {
        return;
    }
    close_requested_ = false;
    CloseResultsPage();
}

void SearchOverlay::CloseResultsPage() {
    if (plugin_ == NULL || results_page_ == NULL) {
        notification_corner_.Restore();
        UpdateHomePadLock();
        return;
    }
    close_requested_ = false;
    close_defer_frames_ = 0;
    home_close_pending_ = false;
    home_pad_press_pending_ = false;
    if (icon_cache_ != NULL) {
        icon_cache_->SetNotificationsEnabled(false);
    }
    if (search_box_ != NULL && search_box_->IsEditing()) {
        search_box_->EndEdit();
    }
    ime_transition_active_ = false;
    ime_editing_seen_ = false;
    results_pending_display_ = false;
    ime_close_pending_ = false;

    if (result_list_ != NULL) {
        result_list_->DisableEvent(
            paf::ui::EV_TOUCH |
            paf::ui::EV_GESTURE_ALL |
            paf::ui::EV_KEYCODE |
            paf::ui::EV_ANALOG |
            paf::ui::EV_FOCUS);
        result_list_->ReleaseFocus();
        result_list_->Clear();
    }
    if (icon_cache_ != NULL) {
        icon_cache_->ResetPins();
    }

    paf::Plugin::PageCloseParam close_param;
    close_param.fade = false;
    plugin_->PageClose("page_search_results", close_param);
    const bool notification_restored = notification_corner_.Restore();
    QueueInteractionLogResult(
        "notification corner restore",
        notification_restored ? 1 : 0);

    results_page_ = NULL;
    search_box_ = NULL;
    results_backdrop_ = NULL;
    results_surface_ = NULL;
    results_ime_surface_ = NULL;
    result_list_ = NULL;
    empty_text_ = NULL;
    result_count_ = NULL;
    close_button_ = NULL;
    results_.clear();
    normalized_query_.clear();
    results_editing_layout_ = false;
    results_draw_priority_ = kResultsDisplayDrawPriority;
    UpdateVisibility();
    UpdateHomePadLock();
}

void SearchOverlay::SubmitSearch() {
    if (search_box_ == NULL) {
        return;
    }
    RefreshSearchFromText(false);
    ime_close_pending_ = false;
    results_pending_display_ = true;
    QueueInteractionLog("search submit end edit");
    search_box_->EndEdit();
}

void SearchOverlay::RefreshSearchFromText(bool force) {
    if (search_box_ == NULL || result_list_ == NULL) {
        return;
    }

    paf::wstring query_wide;
    search_box_->GetString(query_wide);
    paf::string query_utf8;
    if (paf::common::Utf16ToUtf8(query_wide, &query_utf8) < 0) {
        return;
    }

    const String query(query_utf8.c_str());
    const String normalized = NormalizeForSearch(query);
    if (!force && normalized == normalized_query_) {
        return;
    }

    Vector<SearchResult> next_results;
    if (normalized.empty()) {
        recent_apps_.Resolve(index_.apps(), next_results);
    } else {
        index_.Search(query, next_results, 128);
    }
    const std::size_t old_count = result_list_->GetCellNum(0);
    if (old_count != 0) {
        result_list_->DeleteCell(0, 0, old_count);
    }
    normalized_query_ = normalized;
    results_.clear();
    results_.swap(next_results);
    results_pending_display_ = true;
    QueueInteractionLogResult("search result count", static_cast<int>(results_.size()));
    RebuildResultList();
}

void SearchOverlay::RebuildResultList() {
    if (result_list_ == NULL || empty_text_ == NULL) {
        return;
    }

    if (!results_.empty()) {
        result_list_->InsertCell(0, 0, results_.size());
        if (!results_editing_layout_) {
            result_list_->SetFocus(0, 0, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, 0);
        }
    }
    UpdateResultCount();
    UpdateResultPresentation();
}

void SearchOverlay::UpdateResultPresentation() {
    if (result_list_ == NULL || empty_text_ == NULL) {
        return;
    }
    if (!results_.empty()) {
        result_list_->Show(paf::common::transition::Type_FadeinFast);
        empty_text_->Hide(paf::common::transition::Type_FadeinFast);
    } else {
        result_list_->Hide(paf::common::transition::Type_FadeinFast);
        if (normalized_query_.empty()) {
            SetText(empty_text_, "No recent apps");
        } else {
            SetText(empty_text_, "No matching applications");
        }
        empty_text_->Show(paf::common::transition::Type_FadeinFast);
    }
}

void SearchOverlay::UpdateImeTransition() {
    if (search_box_ == NULL) {
        return;
    }

    const bool editing_before_update = search_box_->IsEditing();
    if (ShouldAdoptExternalImeEdit(
            results_page_ != NULL,
            ime_transition_active_,
            editing_before_update)) {
        QueueInteractionLog("ime external edit adopted");
        SetResultsEditingLayout();
        SetResultsDrawPriority(kResultsEditingDrawPriority);
        ime_transition_active_ = true;
        ime_editing_seen_ = true;
        results_pending_display_ = true;
        ime_close_pending_ = false;
    }
    if (!ime_transition_active_) {
        return;
    }

    RefreshSearchFromText(false);
    const bool is_editing = search_box_->IsEditing();
    const bool shell_ready = IsBubbleHomeVisible();
    const ImeFrameAction action = ComputeImeFrameAction(
        ime_transition_active_,
        ime_editing_seen_,
        is_editing,
        results_pending_display_,
        ime_close_pending_,
        shell_ready);
    if (action == ImeFrameAction::MarkEditing) {
        ime_editing_seen_ = true;
        return;
    }
    if (action == ImeFrameAction::ShowResults) {
        QueueInteractionLog("ime transition show results");
        ime_transition_active_ = false;
        ime_editing_seen_ = false;
        results_pending_display_ = false;
        ime_close_pending_ = false;
        SetResultsDrawPriority(kResultsDisplayDrawPriority);
        SetResultsDisplayLayout();
        QueueInteractionLog("ime transition show results returned");
        return;
    }
    if (action == ImeFrameAction::CloseResults) {
        QueueInteractionLog("ime transition close results");
        ime_transition_active_ = false;
        RequestCloseResultsPage();
    }
}

void SearchOverlay::SetResultsDrawPriority(int priority) {
    if (plugin_ == NULL || results_page_ == NULL || results_draw_priority_ == priority) {
        return;
    }
    paf_page_order::SetPagePriority(
        plugin_,
        results_page_,
        "page_search_results",
        priority);
    results_draw_priority_ = priority;
}

void SearchOverlay::SetResultsEditingLayout() {
    results_editing_layout_ = true;
    if (results_backdrop_ != NULL) {
        results_backdrop_->Show(paf::common::transition::Type_Reset);
    }
    if (results_surface_ != NULL) {
        results_surface_->Show(paf::common::transition::Type_Reset);
    }
    if (results_ime_surface_ != NULL) {
        results_ime_surface_->Hide(paf::common::transition::Type_Reset);
    }
    if (result_count_ != NULL) {
        result_count_->Show(paf::common::transition::Type_Reset);
    }
    if (close_button_ != NULL) {
        close_button_->Hide(paf::common::transition::Type_Reset);
    }
    if (result_list_ != NULL) {
        result_list_->SetSize(960.0f, 448.0f, 0.0f);
        result_list_->SetPos(0.0f, -48.0f, 0.0f);
        result_list_->SyncViewSize();
        result_list_->SetViewBox(paf::math::v4(-480.0f, 224.0f, 480.0f, -224.0f));
        result_list_->DisableEvent(paf::ui::EV_FOCUS);
        result_list_->ReleaseFocus();
    }
    if (empty_text_ != NULL) {
        empty_text_->SetPos(0.0f, -48.0f, 0.0f);
    }
    UpdateResultPresentation();
}

void SearchOverlay::SetResultsDisplayLayout() {
    QueueInteractionLog("results display layout begin");
    results_editing_layout_ = false;
    if (results_backdrop_ != NULL) {
        results_backdrop_->Show(paf::common::transition::Type_Reset);
    }
    if (results_surface_ != NULL) {
        results_surface_->Show(paf::common::transition::Type_Reset);
    }
    if (results_ime_surface_ != NULL) {
        results_ime_surface_->Hide(paf::common::transition::Type_Reset);
    }
    if (result_count_ != NULL) {
        result_count_->Show(paf::common::transition::Type_Reset);
    }
    if (close_button_ != NULL) {
        close_button_->Show(paf::common::transition::Type_Reset);
    }
    if (result_list_ != NULL) {
        result_list_->SetSize(960.0f, 448.0f, 0.0f);
        result_list_->SetPos(0.0f, -48.0f, 0.0f);
        result_list_->SyncViewSize();
        result_list_->SetViewBox(paf::math::v4(-480.0f, 224.0f, 480.0f, -224.0f));
        result_list_->EnableEvent(paf::ui::EV_FOCUS);
    }
    if (empty_text_ != NULL) {
        empty_text_->SetPos(0.0f, -48.0f, 0.0f);
    }
    UpdateResultPresentation();
    if (result_list_ != NULL && !results_.empty()) {
        QueueInteractionLog("results display focus begin");
        result_list_->SetFocus(0, 0, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, 0);
        QueueInteractionLog("results display focus returned");
    }
    QueueInteractionLog("results display layout returned");
}

void SearchOverlay::UpdateVisibility() {
    if (home_page_ == NULL) {
        return;
    }
    if (results_page_ != NULL) {
        notification_corner_.Suppress();
    } else {
        notification_corner_.Restore();
    }

    const bool shell_allows_overlay = ShellAllowsOverlay();
    const bool home_surface_visible = IsBubbleHomeVisible();
    if (ShouldCloseResultsPage(
            shell_allows_overlay,
            home_surface_visible,
            results_page_ != NULL,
            ime_transition_active_)) {
        RequestCloseResultsPage();
        return;
    }

    const HomeVisibilityAction action = ComputeHomeVisibilityAction(
        home_visible_,
        home_surface_visible,
        results_page_ != NULL);
    if (action == HomeVisibilityAction::None) {
        return;
    }

    home_visible_ = action == HomeVisibilityAction::Show;
    if (home_visible_) {
        plugin_->PageActivate("page_home_search");
        home_page_->Show(paf::common::transition::Type_FadeinFast);
    } else {
        home_page_->Hide(paf::common::transition::Type_Reset);
        plugin_->PageInactivate("page_home_search");
    }
}

void SearchOverlay::UpdateResultCount() {
    if (result_count_ == NULL) {
        return;
    }
    if (normalized_query_.empty()) {
        SetText(result_count_, "Recent");
        return;
    }
    char text[32];
    sceClibSnprintf(
        text,
        sizeof(text),
        results_.size() == 1 ? "%u result" : "%u results",
        static_cast<unsigned int>(results_.size()));
    SetText(result_count_, text);
}

bool SearchOverlay::IsBubbleHomeVisible() const {
    if (!ShellAllowsOverlay()) {
        return false;
    }
    paf::Plugin* topmenu = paf::Plugin::Find("topmenu_plugin");
    if (topmenu == NULL) {
        return false;
    }
    paf::ui::Scene* root = topmenu->PageRoot(kTopMenuHomePageHash);
    if (root == NULL || !root->IsVisible()) {
        return false;
    }
    paf::ui::Widget* scroll_widget = root->FindChild(kTopMenuHomeScrollHash);
    if (scroll_widget == NULL) {
        return false;
    }
    paf::ui::ScrollView* scroll = static_cast<paf::ui::ScrollView*>(scroll_widget);
    const paf::math::v2 focus = scroll->GetViewFocus();
    const paf::math::v2 target = scroll->GetViewFocusTarget();
    return IsBubbleHomeSurface(
        kTopMenuHomePageHash,
        focus.extract_x(),
        focus.extract_y(),
        target.extract_x(),
        target.extract_y());
}

int SearchOverlay::Launch(const String& title_id) {
    recent_apps_.RetainAvailable(index_.apps());
    recent_apps_.RecordSelection(title_id);
    const int save_result = recent_apps_file_.Save(recent_apps_);
    QueueInteractionLogResult("recent apps save", save_result);

    const int result = OpenTitleLiveArea(title_id);
    QueueInteractionLogResult("result launch", result);
    if (result >= 0) {
        RequestCloseResultsPage();
    } else if (normalized_query_.empty() && result_list_ != NULL) {
        Vector<SearchResult> next_results;
        recent_apps_.Resolve(index_.apps(), next_results);
        const std::size_t old_count = result_list_->GetCellNum(0);
        if (old_count != 0) {
            result_list_->DeleteCell(0, 0, old_count);
        }
        results_.clear();
        results_.swap(next_results);
        RebuildResultList();
    }
    return result;
}

const SearchResult* SearchOverlay::ResultAt(std::size_t index) const {
    return index < results_.size() ? &results_[index] : NULL;
}

void SearchOverlay::ApplyIcon(paf::ui::Widget* button, const String& title_id) {
    if (button == NULL || icon_cache_ == NULL) {
        return;
    }

    paf::ui::Widget* icon = button->FindChild("app_icon_result");
    if (icon == NULL) {
        return;
    }

    paf::intrusive_ptr<paf::graph::Surface> surface = icon_cache_->Get(title_id);
    if (surface.get() != NULL) {
        icon->SetTexture(surface);
        icon->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        icon->Show(paf::common::transition::Type_FadeinFast);
        return;
    }

    icon->SetColor(0.16f, 0.18f, 0.22f, 1.0f);

    const Vector<AppRecord>& apps = index_.apps();
    for (std::size_t index = 0; index < apps.size(); ++index) {
        if (apps[index].title_id == title_id) {
            icon_cache_->Request(title_id, apps[index].icon_path);
            return;
        }
    }
}

void SearchOverlay::SetText(paf::ui::Widget* widget, const String& text) {
    if (widget == NULL) {
        return;
    }
    paf::wstring wide;
    if (paf::common::Utf8ToUtf16(text.c_str(), &wide) >= 0) {
        widget->SetString(wide);
    }
}

bool IsPafPluginVisible(paf::Plugin* plugin) {
    if (plugin == NULL || plugin->GetResource() == NULL) {
        return false;
    }

    paf::cxml::Element page =
        plugin->GetResource()->GetTableElement(paf::resource::TableType_Page).GetFirstChild();
    while (page == true) {
        std::uint32_t page_id = 0;
        paf::cxml::util::GetIDHash(page, "id", &page_id);
        paf::ui::Scene* root = plugin->PageRoot(page_id);
        if (root != NULL && root->IsVisible()) {
            return true;
        }
        page = page.GetNextSibling();
    }
    return false;
}

bool IsTopMenuHomeRenderReady(paf::Plugin* plugin) {
    if (plugin == NULL) {
        return false;
    }
    paf::ui::Scene* root = plugin->PageRoot(kTopMenuHomePageHash);
    if (root == NULL || !root->IsVisible()) {
        return false;
    }
    paf::ui::Widget* scroll_widget = root->FindChild(kTopMenuHomeScrollHash);
    if (scroll_widget == NULL) {
        return false;
    }
    paf::ui::ScrollView* scroll = static_cast<paf::ui::ScrollView*>(scroll_widget);
    const paf::math::v2 focus = scroll->GetViewFocus();
    const paf::math::v2 target = scroll->GetViewFocusTarget();
    const bool bubble_home = IsBubbleHomeSurface(
        kTopMenuHomePageHash,
        focus.extract_x(),
        focus.extract_y(),
        target.extract_x(),
        target.extract_y());
    return bubble_home;
}

void SearchOverlay::OnHomeSearch(
    int,
    paf::ui::Handler*,
    paf::ui::Event*,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL) {
        QueueInteractionLog("home search callback");
        QueueInteractionLogResult("open results page", overlay->OpenResultsPage());
    }
}

void SearchOverlay::OnSearchSubmit(
    int,
    paf::ui::Handler*,
    paf::ui::Event*,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL) {
        QueueInteractionLog("search submit callback");
        overlay->SubmitSearch();
    }
}

void SearchOverlay::OnSearchBoxTap(
    int,
    paf::ui::Handler*,
    paf::ui::Event*,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL && overlay->search_box_ != NULL &&
        !overlay->search_box_->IsEditing()) {
        QueueInteractionLog("search box reopen callback");
        QueueInteractionLogResult("ime reopen", overlay->BeginImeEdit());
    }
}

void SearchOverlay::OnClose(
    int,
    paf::ui::Handler*,
    paf::ui::Event*,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL) {
        QueueInteractionLog("close results callback");
        overlay->HandleBack();
    }
}

void SearchOverlay::OnBackKey(
    int,
    paf::ui::Handler*,
    paf::ui::Event* event,
    void* user_data) {
    if (event == NULL ||
        event->GetDetail() != paf::inputdevice::pad::Data::Keycode::PAD_ESCAPE) {
        return;
    }
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL) {
        QueueInteractionLog("back key callback");
        overlay->HandleBack();
    }
}

void SearchOverlay::OnResultsKeyPress(
    int,
    paf::ui::Handler*,
    paf::ui::Event* event,
    void* user_data) {
    if (event == NULL ||
        event->GetDetail() != paf::inputdevice::pad::Data::Keycode::PAD_PS) {
        return;
    }
    event->Consume();
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay != NULL) {
        QueueInteractionLog("home key callback");
        overlay->HandleHome();
    }
}

void SearchOverlay::OnResultDecide(
    int,
    paf::ui::Handler* handler,
    paf::ui::Event*,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    paf::ui::Widget* widget = static_cast<paf::ui::Widget*>(handler);
    if (overlay == NULL || widget == NULL) {
        QueueInteractionLog("result decide callback missing context");
        return;
    }
    const paf::string title_id = widget->GetName().GetID();
    char line[96];
    sceClibSnprintf(line, sizeof(line), "result decide callback: %s", title_id.c_str());
    QueueInteractionLog(line);
    overlay->Launch(String(title_id.c_str()));
}

void SearchOverlay::OnIconReady(
    int,
    paf::ui::Handler* handler,
    paf::ui::Event* event,
    void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    paf::ui::Widget* widget = static_cast<paf::ui::Widget*>(handler);
    if (overlay == NULL || widget == NULL || event == NULL) {
        return;
    }
    if (static_cast<std::uint32_t>(event->GetValue(0)) != widget->GetName().GetIDHash()) {
        return;
    }
    const paf::string title_id = widget->GetName().GetID();
    overlay->ApplyIcon(widget, String(title_id.c_str()));
}

void SearchOverlay::HomePadDeviceHandler(paf::inputdevice::pad::Data* data) {
    SearchOverlay* overlay = home_pad_overlay_;
    if (overlay != NULL && data != NULL) {
        if (ObserveHomePadPress(
                overlay->results_page_ != NULL,
                paf::inputdevice::pad::Data::Keycode::PAD_PS,
                data->paddata,
                overlay->home_ps_owned_)) {
            overlay->home_pad_press_pending_ = true;
        }
    }
    if (previous_home_pad_handler_ != NULL &&
        previous_home_pad_handler_ != HomePadDeviceHandler) {
        previous_home_pad_handler_(data);
    }
}

void SearchOverlay::VisibilityTick(void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay == NULL || !overlay->visibility_registered_) {
        return;
    }
    if (overlay->home_pad_press_pending_) {
        overlay->home_pad_press_pending_ = false;
        QueueInteractionLog("home pad device callback");
        overlay->HandleHome();
    }
    overlay->UpdateImeTransition();
    if (overlay->visibility_registered_) {
        overlay->UpdateVisibility();
    }
    if (overlay->visibility_registered_) {
        overlay->ProcessCloseRequest();
    }
    if (overlay->visibility_registered_) {
        overlay->UpdateHomePadLock();
    }
}

} // namespace livearea_search

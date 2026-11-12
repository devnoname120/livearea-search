#include "search_overlay.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <taihen.h>

#include <livearea_search/overlay_visibility.h>
#if LIVEAREA_SEARCH_DIAGNOSTICS
#include <livearea_search/deferred_log_queue.h>
#endif

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

IndicatorPopupVisibility QueryIndicatorPopupVisibility(paf::Plugin* indicator) {
    if (indicator == NULL) {
        return ClassifyIndicatorPopupOwnership(
            false,
            false,
            ClassifyIndicatorPopupVisibility(false, 0, 0U));
    }
    paf::vector<paf::string> page_ids;
    const int result = indicator->GetOpenedPages(&page_ids);
    const IndicatorPopupVisibility opened_pages =
        ClassifyIndicatorPopupVisibility(
        true,
        result,
        page_ids.size());
    paf::ui::Scene* static_root = indicator->PageRoot(kIndicatorPageHash);
    paf::ui::Widget* notification_corner = static_root == NULL ? NULL :
        static_root->FindChild(kIndicatorNotificationCornerHash);
    return ClassifyIndicatorPopupOwnership(
        static_root != NULL,
        notification_corner != NULL,
        opened_pages);
}

#if LIVEAREA_SEARCH_DIAGNOSTICS
unsigned int HashPluginPageState(
    unsigned int hash,
    const char* page_id,
    bool root_found,
    bool visible) {
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(page_id == NULL ? "" : page_id);
    while (*cursor != 0U) {
        hash ^= *cursor++;
        hash *= 16777619U;
    }
    hash ^= root_found ? 1U : 0U;
    hash *= 16777619U;
    hash ^= visible ? 1U : 0U;
    return hash * 16777619U;
}

void LogPluginPages(
    const char* label,
    paf::Plugin* plugin,
    unsigned int& last_state) {
    paf::vector<paf::string> page_ids;
    const int opened_result =
        plugin == NULL ? -1 : plugin->GetOpenedPages(&page_ids);
    unsigned int state = 2166136261U;
    state ^= static_cast<unsigned int>(opened_result);
    state *= 16777619U;
    state ^= static_cast<unsigned int>(page_ids.size());
    state *= 16777619U;
    unsigned int resource_count = 0U;
    if (plugin != NULL && plugin->GetResource() != NULL) {
        paf::cxml::Element page =
            plugin->GetResource()->GetTableElement(
                paf::resource::TableType_Page).GetFirstChild();
        while (page == true) {
            std::uint32_t page_id = 0U;
            paf::cxml::util::GetIDHash(page, "id", &page_id);
            paf::ui::Scene* root = plugin->PageRoot(page_id);
            state ^= page_id;
            state *= 16777619U;
            state ^= root != NULL ? 1U : 0U;
            state *= 16777619U;
            state ^= root != NULL && root->IsVisible() ? 1U : 0U;
            state *= 16777619U;
            ++resource_count;
            page = page.GetNextSibling();
        }
    }
    for (std::size_t index = 0; index < page_ids.size(); ++index) {
        paf::ui::Scene* root = plugin->PageRoot(paf::IDParam(page_ids[index]));
        state = HashPluginPageState(
            state,
            page_ids[index].c_str(),
            root != NULL,
            root != NULL && root->IsVisible());
    }
    if (state == last_state) {
        return;
    }
    char line[DeferredLogQueue::kLineCapacity];
    sceClibSnprintf(
        line,
        sizeof(line),
        "%s pages resource=%u opened=0x%08X/%u",
        label,
        resource_count,
        static_cast<unsigned int>(opened_result),
        static_cast<unsigned int>(page_ids.size()));
    QueueInteractionLog(line);
    if (plugin != NULL && plugin->GetResource() != NULL) {
        paf::cxml::Element page =
            plugin->GetResource()->GetTableElement(
                paf::resource::TableType_Page).GetFirstChild();
        unsigned int index = 0U;
        while (page == true) {
            std::uint32_t page_id = 0U;
            paf::cxml::util::GetIDHash(page, "id", &page_id);
            paf::ui::Scene* root = plugin->PageRoot(page_id);
            if (root != NULL && root->IsVisible()) {
                sceClibSnprintf(
                    line,
                    sizeof(line),
                    "%s resource[%u]=0x%08X visible=1",
                    label,
                    index,
                    static_cast<unsigned int>(page_id));
                QueueInteractionLog(line);
            }
            ++index;
            page = page.GetNextSibling();
        }
    }
    for (std::size_t index = 0; index < page_ids.size(); ++index) {
        paf::ui::Scene* root = plugin->PageRoot(paf::IDParam(page_ids[index]));
        sceClibSnprintf(
            line,
            sizeof(line),
            "%s opened[%u]=%s root=%d visible=%d",
            label,
            static_cast<unsigned int>(index),
            page_ids[index].c_str(),
            root != NULL ? 1 : 0,
            root != NULL && root->IsVisible() ? 1 : 0);
        QueueInteractionLog(line);
    }
    last_state = state;
}

void LogHomeGate(
    bool shell_allows_overlay,
    bool event_notification_visible,
    bool livearea_visible,
    bool topmenu_found,
    bool root_found,
    bool root_visible,
    bool scroll_found,
    bool bubble_home_visible,
    bool exposed) {
    const unsigned int state =
        (shell_allows_overlay ? 1U : 0U) |
        (event_notification_visible ? 1U << 1U : 0U) |
        (livearea_visible ? 1U << 2U : 0U) |
        (topmenu_found ? 1U << 3U : 0U) |
        (root_found ? 1U << 4U : 0U) |
        (root_visible ? 1U << 5U : 0U) |
        (scroll_found ? 1U << 6U : 0U) |
        (bubble_home_visible ? 1U << 7U : 0U) |
        (exposed ? 1U << 8U : 0U);
    static unsigned int last_state = ~0U;
    if (state == last_state) {
        return;
    }
    char line[192];
    sceClibSnprintf(
        line,
        sizeof(line),
        "home gate shell=%d notification=%d livearea=%d topmenu=%d root=%d root_visible=%d scroll=%d bubble=%d exposed=%d",
        shell_allows_overlay ? 1 : 0,
        event_notification_visible ? 1 : 0,
        livearea_visible ? 1 : 0,
        topmenu_found ? 1 : 0,
        root_found ? 1 : 0,
        root_visible ? 1 : 0,
        scroll_found ? 1 : 0,
        bubble_home_visible ? 1 : 0,
        exposed ? 1 : 0);
    QueueInteractionLog(line);
    last_state = state;
}

#endif

} // namespace

SearchOverlay* SearchOverlay::home_pad_overlay_ = NULL;
paf_pad_device_handler::DeviceHandler SearchOverlay::previous_home_pad_handler_ = NULL;
paf_touchscreen_device_handler::DeviceHandler SearchOverlay::previous_touchscreen_handler_ = NULL;

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
      notification_panel_suppression_(NotificationPanelSuppression::Inactive),
      notification_panel_opening_deadline_us_(0),
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
    if (previous_touchscreen_handler_ != NULL) {
        const paf_touchscreen_device_handler::DeviceHandler displaced_touchscreen =
            paf_touchscreen_device_handler::SetDeviceHandler(
                0U,
                previous_touchscreen_handler_);
        if (displaced_touchscreen != TouchscreenDeviceHandler) {
            paf_touchscreen_device_handler::SetDeviceHandler(
                0U,
                displaced_touchscreen);
        }
        previous_touchscreen_handler_ = NULL;
    }
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
    previous_touchscreen_handler_ =
        paf_touchscreen_device_handler::SetDeviceHandler(0U, TouchscreenDeviceHandler);
#if LIVEAREA_SEARCH_DIAGNOSTICS
    StartupLogResult(
        "overlay previous touchscreen handler",
        previous_touchscreen_handler_ != NULL ? 1 : 0);
#endif
    if (previous_touchscreen_handler_ == NULL ||
        previous_touchscreen_handler_ == TouchscreenDeviceHandler) {
        paf_touchscreen_device_handler::SetDeviceHandler(
            0U,
            previous_touchscreen_handler_);
        previous_touchscreen_handler_ = NULL;
        return -1;
    }

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
    home_button_->DisableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);
    home_button_->ReleaseFocus();
    home_page_->Hide(paf::common::transition::Type_Reset);
    plugin_->PageInactivate("page_home_search");
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
        paf::inputdevice::pad::Data::Keycode::PAD_ESCAPE);
    results_page_->AddEventCallback(
        paf::ui::Handler::CB_KEYCODE_PRESS,
        OnBackKey,
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
    return ResolveShellUtilFunction(kShellUtilUnlockNid, shell_util_unlock_);
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
}

bool SearchOverlay::AcquireHomePadLock() {
    if (home_ps_lock_held_) {
        return true;
    }
    if (!shell_util_ready_ || shell_util_lock_ == NULL) {
        return false;
    }
    const int result = shell_util_lock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2);
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
    const int result = shell_util_unlock_(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN_2);
    QueueInteractionLogResult("home PS unlock", result);
    if (result >= 0) {
        home_ps_lock_held_ = false;
    }
}

void SearchOverlay::UpdateHomePadClose() {
    const HomePadCloseAction action = ComputeHomePadCloseAction(
        home_close_pending_,
        home_ps_owned_);
    if (action != HomePadCloseAction::ReleaseLockAndClose) {
        return;
    }
    ReleaseHomePadLock();
    RequestCloseResultsPage();
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
        ReleaseHomePadLock();
        notification_corner_.Restore();
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
    ReleaseHomePadLock();
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

    SetHomeVisible(action == HomeVisibilityAction::Show);
}

void SearchOverlay::SetHomeVisible(bool visible) {
    if (home_visible_ == visible || home_button_ == NULL ||
        home_page_ == NULL || plugin_ == NULL) {
        return;
    }
    home_visible_ = visible;
    if (home_visible_) {
        home_button_->EnableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);
        plugin_->PageActivate("page_home_search");
        home_page_->Show(paf::common::transition::Type_FadeinFast);
    } else {
        home_button_->DisableEvent(paf::ui::EV_TOUCH | paf::ui::EV_GESTURE_ALL);
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

bool SearchOverlay::IsBubbleHomeVisible() {
    const bool shell_allows_overlay = ShellAllowsOverlay();
    paf::Plugin* indicator = shell_allows_overlay ?
        paf::Plugin::Find("indicator_plugin") : NULL;
    const IndicatorPopupVisibility popup_visibility =
        QueryIndicatorPopupVisibility(indicator);
    const bool notification_touch_suppressed =
        AdvanceNotificationPanelSuppression(
            popup_visibility,
            static_cast<std::uint64_t>(sceKernelGetSystemTimeWide()),
            notification_panel_opening_deadline_us_,
            notification_panel_suppression_);
    paf::ui::Scene* event_notification = indicator == NULL ? NULL :
        indicator->PageRoot(paf::IDParam("page_event_notification"));
    const bool event_notification_visible =
        (event_notification != NULL && event_notification->IsVisible()) ||
        popup_visibility == IndicatorPopupVisibility::Visible ||
        notification_touch_suppressed;
    paf::Plugin* livearea = shell_allows_overlay ?
        paf::Plugin::Find("livearea_plugin") : NULL;
#if LIVEAREA_SEARCH_DIAGNOSTICS
    static unsigned int last_indicator_pages = ~0U;
    static unsigned int last_livearea_pages = ~0U;
    LogPluginPages("indicator", indicator, last_indicator_pages);
    LogPluginPages("livearea", livearea, last_livearea_pages);
#endif
    const bool livearea_visible = IsPafPluginVisible(livearea);
    paf::Plugin* topmenu = shell_allows_overlay ?
        paf::Plugin::Find("topmenu_plugin") : NULL;
    paf::ui::Scene* root = topmenu == NULL ? NULL :
        topmenu->PageRoot(kTopMenuHomePageHash);
    const bool root_visible = root != NULL && root->IsVisible();
    paf::ui::Widget* scroll_widget = root_visible ?
        root->FindChild(kTopMenuHomeScrollHash) : NULL;
    bool bubble_home_visible = false;
    if (scroll_widget != NULL) {
        paf::ui::ScrollView* scroll = static_cast<paf::ui::ScrollView*>(scroll_widget);
        const paf::math::v2 focus = scroll->GetViewFocus();
        const paf::math::v2 target = scroll->GetViewFocusTarget();
        bubble_home_visible = IsBubbleHomeSurface(
            kTopMenuHomePageHash,
            focus.extract_x(),
            focus.extract_y(),
            target.extract_x(),
            target.extract_y());
    }
    const bool exposed = ShouldExposeHomeSearch(
        shell_allows_overlay,
        bubble_home_visible,
        event_notification_visible,
        livearea_visible);
#if LIVEAREA_SEARCH_DIAGNOSTICS
    LogHomeGate(
        shell_allows_overlay,
        event_notification_visible,
        livearea_visible,
        topmenu != NULL,
        root != NULL,
        root_visible,
        scroll_widget != NULL,
        bubble_home_visible,
        exposed);
#endif
    return exposed;
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
    if (overlay != NULL && overlay->IsBubbleHomeVisible()) {
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
#if LIVEAREA_SEARCH_DIAGNOSTICS
    char line[96];
    sceClibSnprintf(line, sizeof(line), "result decide callback: %s", title_id.c_str());
    QueueInteractionLog(line);
#endif
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
    paf::inputdevice::pad::Data forwarded;
    paf::inputdevice::pad::Data* forwarded_data = data;
    if (overlay != NULL && data != NULL) {
        forwarded = *data;
        forwarded_data = &forwarded;
        if (ObserveHomePadPress(
                overlay->results_page_ != NULL,
                paf::inputdevice::pad::Data::Keycode::PAD_PS,
                forwarded.paddata,
                overlay->home_ps_owned_)) {
            overlay->home_pad_press_pending_ = true;
        }
    }
    if (previous_home_pad_handler_ != NULL &&
        previous_home_pad_handler_ != HomePadDeviceHandler) {
        previous_home_pad_handler_(forwarded_data);
    }
}

void SearchOverlay::TouchscreenDeviceHandler(
    paf_touchscreen_device_handler::DataList* data) {
    SearchOverlay* overlay = home_pad_overlay_;
    if (data != NULL) {
        for (std::size_t slot = 0; slot < 8U; ++slot) {
            const paf_touchscreen_device_handler::DataRecord& record =
                data->records[slot];
#if LIVEAREA_SEARCH_DIAGNOSTICS
            if (record.state != 1 && record.state != 2) {
                continue;
            }
            char line[DeferredLogQueue::kLineCapacity];
            sceClibSnprintf(
                line,
                sizeof(line),
                "touch front slot=%u state=%d x=%d y=%d",
                static_cast<unsigned int>(slot),
                record.state,
                record.x,
                record.y);
            QueueInteractionLog(line);
#endif
            if (overlay != NULL && ObserveNotificationCornerPress(
                    overlay->results_page_ != NULL,
                    record.state == 1,
                    record.x,
                    record.y,
                    static_cast<std::uint64_t>(sceKernelGetSystemTimeWide()),
                    overlay->notification_panel_suppression_,
                    overlay->notification_panel_opening_deadline_us_)) {
                overlay->SetHomeVisible(false);
            }
        }
    }
    if (previous_touchscreen_handler_ != NULL &&
        previous_touchscreen_handler_ != TouchscreenDeviceHandler) {
        previous_touchscreen_handler_(data);
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
        overlay->UpdateHomePadClose();
    }
    if (overlay->visibility_registered_) {
        overlay->ProcessCloseRequest();
    }
}

} // namespace livearea_search

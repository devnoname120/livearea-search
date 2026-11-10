#include "search_overlay.h"

#include <psp2/kernel/clib.h>

#include <livearea_search/overlay_visibility.h>

#include "launcher.h"
#include "paf_memory.h"
#include "shell_state.h"
#include "startup_log.h"

namespace livearea_search {

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
      icon_cache_(NULL),
      visibility_registered_(false),
      home_visible_(false),
      ime_transition_active_(false),
      ime_editing_seen_(false),
      results_pending_display_(false),
      ime_close_pending_(false),
      results_editing_layout_(false),
      close_requested_(false),
      close_defer_frames_(0),
      initialized_(false) {}

SearchOverlay::~SearchOverlay() {
    visibility_registered_ = false;
    paf::common::MainThreadCallList::Unregister(VisibilityTick, this);
    CloseResultsPage();

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

int SearchOverlay::Initialize() {
    StartupLog("overlay initialize begin");
    if (initialized_) {
        return 0;
    }
    if (plugin_ == NULL) {
        return -1;
    }

    int result = index_source_.Resolve();
    StartupLogResult("overlay lsdb resolve", result);
    if (result < 0) {
        return result;
    }
    result = index_source_.Load(index_);
    StartupLogResult("overlay lsdb load", result);
    if (result < 0) {
        return result;
    }

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

    paf::Plugin::PageOpenParam open_param;
    open_param.fade = true;
    open_param.fade_time_ms = 120.0f;
    results_page_ = plugin_->PageOpen("page_search_results", open_param);
    if (results_page_ == NULL) {
        return -1;
    }

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
    result_list_->SetCellSizeDefault(0, paf::math::v4(920.0f, 70.0f));
    result_list_->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST);
    result_list_->SetScrollType(paf::ui::ListView::SCROLL_TYPE_VERTICAL);

    results_.clear();
    normalized_query_.clear();
    empty_text_->Hide(paf::common::transition::Type_Reset);
    UpdateResultCount();
    SetResultsEditingLayout();
    UpdateVisibility();
    if (results_page_ == NULL || search_box_ == NULL) {
        return -1;
    }
    icon_cache_->SetNotificationsEnabled(true);
    return BeginImeEdit();
}

int SearchOverlay::BeginImeEdit() {
    if (search_box_ == NULL) {
        return -1;
    }

    SetResultsEditingLayout();
    ime_transition_active_ = true;
    ime_editing_seen_ = false;
    results_pending_display_ = !normalized_query_.empty();
    ime_close_pending_ = false;
    const int result = search_box_->BeginEdit();
    QueueInteractionLogResult("ime begin edit", result);
    if (result < 0) {
        ime_transition_active_ = false;
        RequestCloseResultsPage();
    }
    return result;
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
        return;
    }
    close_requested_ = false;
    close_defer_frames_ = 0;
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
    UpdateVisibility();
}

void SearchOverlay::SubmitSearch() {
    if (search_box_ == NULL) {
        return;
    }
    RefreshSearchFromText(false);
    ime_close_pending_ = normalized_query_.empty();
    results_pending_display_ = !normalized_query_.empty();
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
    if (!normalized.empty()) {
        index_.Search(query, next_results, 128);
    }
    const std::size_t old_count = result_list_->GetCellNum(0);
    if (old_count != 0) {
        result_list_->DeleteCell(0, 0, old_count);
    }
    normalized_query_ = normalized;
    results_.clear();
    results_.swap(next_results);
    results_pending_display_ = !normalized_query_.empty();
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
    if (normalized_query_.empty()) {
        result_list_->Hide(paf::common::transition::Type_Reset);
        empty_text_->Hide(paf::common::transition::Type_Reset);
        return;
    }
    if (!results_.empty()) {
        result_list_->Show(paf::common::transition::Type_FadeinFast);
        empty_text_->Hide(paf::common::transition::Type_FadeinFast);
    } else {
        result_list_->Hide(paf::common::transition::Type_FadeinFast);
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
        ime_transition_active_ = true;
        ime_editing_seen_ = true;
        results_pending_display_ = !normalized_query_.empty();
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

void SearchOverlay::SetResultsEditingLayout() {
    results_editing_layout_ = true;
    if (results_backdrop_ != NULL) {
        results_backdrop_->Hide(paf::common::transition::Type_Reset);
    }
    if (results_surface_ != NULL) {
        results_surface_->Hide(paf::common::transition::Type_Reset);
    }
    if (results_ime_surface_ != NULL) {
        results_ime_surface_->Show(paf::common::transition::Type_Reset);
    }
    if (result_count_ != NULL) {
        result_count_->Show(paf::common::transition::Type_Reset);
    }
    if (close_button_ != NULL) {
        close_button_->Hide(paf::common::transition::Type_Reset);
    }
    if (result_list_ != NULL) {
        result_list_->SetSize(920.0f, 170.0f, 0.0f);
        result_list_->SetPos(0.0f, 73.0f, 0.0f);
        result_list_->DisableEvent(paf::ui::EV_FOCUS);
        result_list_->ReleaseFocus();
    }
    if (empty_text_ != NULL) {
        empty_text_->SetPos(0.0f, 73.0f, 0.0f);
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
        result_list_->SetSize(920.0f, 440.0f, 0.0f);
        result_list_->SetPos(0.0f, -36.0f, 0.0f);
        result_list_->EnableEvent(paf::ui::EV_FOCUS);
    }
    if (empty_text_ != NULL) {
        empty_text_->SetPos(0.0f, -10.0f, 0.0f);
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
        home_page_->Show(paf::common::transition::Type_Reset);
    } else {
        home_page_->Hide(paf::common::transition::Type_Reset);
        plugin_->PageInactivate("page_home_search");
    }
}

void SearchOverlay::UpdateResultCount() {
    if (result_count_ == NULL) {
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
    const int result = OpenTitleLiveArea(title_id);
    QueueInteractionLogResult("result launch", result);
    if (result >= 0) {
        RequestCloseResultsPage();
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
        overlay->RequestCloseResultsPage();
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

void SearchOverlay::VisibilityTick(void* user_data) {
    SearchOverlay* overlay = static_cast<SearchOverlay*>(user_data);
    if (overlay == NULL || !overlay->visibility_registered_) {
        return;
    }
    overlay->UpdateImeTransition();
    if (overlay->visibility_registered_) {
        overlay->UpdateVisibility();
    }
    if (overlay->visibility_registered_) {
        overlay->ProcessCloseRequest();
    }
}

} // namespace livearea_search

#pragma once

#include <psp2/shellutil.h>

#include "paf_compat.h"

#include <livearea_search/app_index.h>
#include <livearea_search/overlay_visibility.h>
#include <livearea_search/recent_apps.h>

#include "icon_cache.h"
#include "notification_corner.h"
#include "paf_pad_device_handler.h"
#include "paf_touchscreen_device_handler.h"
#include "recent_apps_file.h"

namespace livearea_search {

bool IsPafPluginVisible(paf::Plugin* plugin);
bool IsTopMenuHomeRenderReady(paf::Plugin* plugin);

class SearchOverlay {
public:
    explicit SearchOverlay(paf::Plugin* plugin);
    ~SearchOverlay();

    int Initialize(AppIndex& prepared_index, RecentApps& prepared_recent_apps);

private:
    typedef int (*ShellUtilInitEventsFunction)(int);
    typedef int (*ShellUtilLockFunction)(SceShellUtilLockType);
    typedef int (*ShellUtilUnlockFunction)(SceShellUtilLockType);

    class ResultItemFactory : public paf::ui::listview::ItemFactory {
    public:
        explicit ResultItemFactory(SearchOverlay* overlay);

        paf::ui::ListItem* Create(CreateParam& param);
        void Start(StartParam& param);
        void Stop(StopParam& param);

    private:
        SearchOverlay* overlay_;
    };

    static void OnHomeSearch(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnSearchSubmit(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnSearchBoxTap(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnClose(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnBackKey(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnResultDecide(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void OnIconReady(
        int id,
        paf::ui::Handler* handler,
        paf::ui::Event* event,
        void* user_data);
    static void HomePadDeviceHandler(paf::inputdevice::pad::Data* data);
    static void TouchscreenDeviceHandler(
        paf_touchscreen_device_handler::DataList* data);
    static void VisibilityTick(void* user_data);

    int OpenHomePage();
    int OpenResultsPage();
    int BeginImeEdit();
    int ResolveShellUtil();
    void HandleBack();
    void HandleHome();
    void HandleStartButton();
    bool AcquireHomePadLock();
    void ReleaseHomePadLock();
    void UpdateHomePadClose();
    void RequestCloseResultsPage();
    void ProcessCloseRequest();
    void CloseResultsPage();
    void SubmitSearch();
    void RefreshSearchFromText(bool force);
    void RebuildResultList();
    void UpdateResultPresentation();
    void UpdateImeTransition();
    void SetResultsDrawPriority(int priority);
    void SetResultsEditingLayout();
    void SetResultsDisplayLayout();
    void UpdateVisibility();
    void SetHomeVisible(bool visible, bool immediate);
    void UpdateResultCount();
    bool IsBubbleHomeVisible(bool& immediate_hide);
    int Launch(const String& title_id);

    const SearchResult* ResultAt(std::size_t index) const;
    void ApplyIcon(paf::ui::Widget* button, const String& title_id);

    static void SetText(paf::ui::Widget* widget, const String& text);

    paf::Plugin* plugin_;
    paf::ui::Scene* home_page_;
    paf::ui::Scene* results_page_;
    paf::ui::CornerButton* home_button_;
    paf::ui::TextBox* search_box_;
    paf::ui::Widget* results_backdrop_;
    paf::ui::Widget* results_surface_;
    paf::ui::Widget* results_ime_surface_;
    paf::ui::ListView* result_list_;
    paf::ui::Text* empty_text_;
    paf::ui::Text* result_count_;
    paf::ui::CornerButton* close_button_;

    AppIndex index_;
    NotificationCornerGuard notification_corner_;
    RecentApps recent_apps_;
    RecentAppsFile recent_apps_file_;
    Vector<SearchResult> results_;
    String normalized_query_;
    IconCache* icon_cache_;
    bool visibility_registered_;
    bool home_visible_;
    NotificationPanelSuppression notification_panel_suppression_;
    std::uint64_t notification_panel_opening_deadline_us_;
    bool ime_transition_active_;
    bool ime_editing_seen_;
    bool results_pending_display_;
    bool ime_close_pending_;
    bool results_editing_layout_;
    int results_draw_priority_;
    bool close_requested_;
    std::uint32_t close_defer_frames_;
    bool home_close_pending_;
    bool home_pad_press_pending_;
    bool home_ps_owned_;
    bool start_button_open_pending_;
    bool start_button_owned_;
    bool shell_util_ready_;
    bool home_ps_lock_held_;
    ShellUtilInitEventsFunction shell_util_init_events_;
    ShellUtilLockFunction shell_util_lock_;
    ShellUtilUnlockFunction shell_util_unlock_;
    bool initialized_;

    static SearchOverlay* home_pad_overlay_;
    static paf_pad_device_handler::DeviceHandler previous_home_pad_handler_;
    static paf_touchscreen_device_handler::DeviceHandler previous_touchscreen_handler_;
};

} // namespace livearea_search

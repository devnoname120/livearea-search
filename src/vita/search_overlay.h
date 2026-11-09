#pragma once

#include "paf_compat.h"

#include <livearea_search/app_index.h>

#include "icon_cache.h"
#include "lsdb_index_source.h"

namespace livearea_search {

bool IsPafPluginVisible(paf::Plugin* plugin);

class SearchOverlay {
public:
    explicit SearchOverlay(paf::Plugin* plugin);
    ~SearchOverlay();

    int Initialize();

private:
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
    static void OnClose(
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
    static void VisibilityTick(void* user_data);

    int OpenHomePage();
    int OpenResultsPage();
    int BeginImeEdit();
    void CloseResultsPage();
    void SubmitSearch();
    void RebuildResultList();
    void UpdateImeTransition();
    void SetResultsChromeVisible(bool visible);
    void UpdateVisibility();
    void UpdateResultCount();
    bool IsTopMenuVisible() const;
    int Launch(const String& title_id);

    const SearchResult* ResultAt(std::size_t index) const;
    void ApplyIcon(paf::ui::Widget* button, const String& title_id);

    static void SetText(paf::ui::Widget* widget, const String& text);

    paf::Plugin* plugin_;
    paf::ui::Scene* home_page_;
    paf::ui::Scene* results_page_;
    paf::ui::Button* home_button_;
    paf::ui::TextBox* search_box_;
    paf::ui::Widget* results_backdrop_;
    paf::ui::Widget* results_surface_;
    paf::ui::ListView* result_list_;
    paf::ui::Text* empty_text_;
    paf::ui::Text* result_count_;
    paf::ui::CornerButton* close_button_;

    AppIndex index_;
    LsdbIndexSource index_source_;
    Vector<SearchResult> results_;
    IconCache* icon_cache_;
    bool visibility_registered_;
    bool home_visible_;
    bool ime_transition_active_;
    bool ime_editing_seen_;
    bool results_pending_display_;
    bool ime_close_pending_;
    bool initialized_;
};

} // namespace livearea_search

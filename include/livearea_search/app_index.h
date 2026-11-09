#pragma once

#include <cstddef>

#include <livearea_search/types.h>

namespace livearea_search {

enum class MatchClass {
    Exact,
    Prefix,
    TokenPrefix,
    Substring,
    Metadata
};

struct AppRecord {
    String title_id;
    String display_title;
    String app_name;
    String category;
    String icon_path;
    bool launchable;

    AppRecord() : launchable(true) {}

    AppRecord(
        const String& title_id_value,
        const String& display_title_value,
        const String& app_name_value,
        const String& category_value,
        const String& icon_path_value,
        bool launchable_value)
        : title_id(title_id_value),
          display_title(display_title_value),
          app_name(app_name_value),
          category(category_value),
          icon_path(icon_path_value),
          launchable(launchable_value) {}
};

struct SearchResult {
    const AppRecord* app;
    MatchClass match_class;

    SearchResult() : app(NULL), match_class(MatchClass::Metadata) {}
    SearchResult(const AppRecord* app_value, MatchClass match_class_value)
        : app(app_value), match_class(match_class_value) {}
};

class AppIndex {
public:
    void Assign(Vector<AppRecord>& apps);

    const Vector<AppRecord>& apps() const;
    void Search(
        const String& query,
        Vector<SearchResult>& results,
        std::size_t limit = 64) const;

private:
    struct IndexedApp {
        std::size_t app_index;
        String normalized_title;
        String normalized_title_id;
        String normalized_name;
        String normalized_category;
        Vector<String> title_tokens;
        Vector<String> name_tokens;
    };

    Vector<AppRecord> apps_;
    Vector<IndexedApp> indexed_apps_;
};

String NormalizeForSearch(const String& text);
void SortAndDeduplicateAppRecords(Vector<AppRecord>& records);

} // namespace livearea_search

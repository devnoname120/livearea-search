#include <livearea_search/app_index.h>

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

namespace livearea_search {

namespace {

bool DecodeUtf8(const String& text, std::size_t& offset, std::uint32_t& codepoint) {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80U) {
        codepoint = first;
        ++offset;
        return true;
    }

    std::size_t length = 0;
    std::uint32_t value = 0;
    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value = static_cast<std::uint32_t>(first & 0x1FU);
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value = static_cast<std::uint32_t>(first & 0x0FU);
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value = static_cast<std::uint32_t>(first & 0x07U);
    } else {
        ++offset;
        return false;
    }

    if (offset + length > text.size()) {
        ++offset;
        return false;
    }

    for (std::size_t index = 1; index < length; ++index) {
        const auto next = static_cast<unsigned char>(text[offset + index]);
        if ((next & 0xC0U) != 0x80U) {
            ++offset;
            return false;
        }
        value = (value << 6U) | static_cast<std::uint32_t>(next & 0x3FU);
    }

    const bool overlong =
        (length == 2 && value < 0x80U) ||
        (length == 3 && value < 0x800U) ||
        (length == 4 && value < 0x10000U);
    if (overlong || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) {
        ++offset;
        return false;
    }

    offset += length;
    codepoint = value;
    return true;
}

void AppendUtf8(std::uint32_t codepoint, String& output) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

void AppendSeparator(String& output) {
    if (!output.empty() && output.back() != ' ') {
        output.push_back(' ');
    }
}

bool In(std::uint32_t value, std::uint32_t first, std::uint32_t last) {
    return value >= first && value <= last;
}

bool AppendLatinFold(std::uint32_t codepoint, String& output) {
    const auto append = [&output](const char* text) {
        output.append(text);
    };

    if (In(codepoint, 0x00C0U, 0x00C5U) || In(codepoint, 0x00E0U, 0x00E5U) ||
        In(codepoint, 0x0100U, 0x0105U)) {
        append("a");
    } else if (codepoint == 0x00C6U || codepoint == 0x00E6U) {
        append("ae");
    } else if (codepoint == 0x00C7U || codepoint == 0x00E7U ||
               In(codepoint, 0x0106U, 0x010DU)) {
        append("c");
    } else if (codepoint == 0x00D0U || codepoint == 0x00F0U ||
               In(codepoint, 0x010EU, 0x0111U)) {
        append("d");
    } else if (In(codepoint, 0x00C8U, 0x00CBU) || In(codepoint, 0x00E8U, 0x00EBU) ||
               In(codepoint, 0x0112U, 0x011BU)) {
        append("e");
    } else if (In(codepoint, 0x011CU, 0x0123U)) {
        append("g");
    } else if (In(codepoint, 0x0124U, 0x0127U)) {
        append("h");
    } else if (In(codepoint, 0x00CCU, 0x00CFU) || In(codepoint, 0x00ECU, 0x00EFU) ||
               In(codepoint, 0x0128U, 0x0131U)) {
        append("i");
    } else if (In(codepoint, 0x0134U, 0x0135U)) {
        append("j");
    } else if (In(codepoint, 0x0136U, 0x0138U)) {
        append("k");
    } else if (In(codepoint, 0x0139U, 0x0142U)) {
        append("l");
    } else if (codepoint == 0x00D1U || codepoint == 0x00F1U ||
               In(codepoint, 0x0143U, 0x014BU)) {
        append("n");
    } else if (In(codepoint, 0x00D2U, 0x00D6U) || codepoint == 0x00D8U ||
               In(codepoint, 0x00F2U, 0x00F6U) || codepoint == 0x00F8U ||
               In(codepoint, 0x014CU, 0x0151U)) {
        append("o");
    } else if (codepoint == 0x0152U || codepoint == 0x0153U) {
        append("oe");
    } else if (In(codepoint, 0x0154U, 0x0159U)) {
        append("r");
    } else if (codepoint == 0x00DFU || In(codepoint, 0x015AU, 0x0161U)) {
        append(codepoint == 0x00DFU ? "ss" : "s");
    } else if (In(codepoint, 0x0162U, 0x0167U)) {
        append("t");
    } else if (In(codepoint, 0x00D9U, 0x00DCU) || In(codepoint, 0x00F9U, 0x00FCU) ||
               In(codepoint, 0x0168U, 0x0173U)) {
        append("u");
    } else if (In(codepoint, 0x0174U, 0x0175U)) {
        append("w");
    } else if (codepoint == 0x00DDU || codepoint == 0x00FDU || codepoint == 0x00FFU ||
               In(codepoint, 0x0176U, 0x0177U)) {
        append("y");
    } else if (In(codepoint, 0x0179U, 0x017EU)) {
        append("z");
    } else if (codepoint == 0x00DEU || codepoint == 0x00FEU) {
        append("th");
    } else {
        return false;
    }
    return true;
}

void AppendFolded(std::uint32_t codepoint, String& output) {
    if (codepoint == 0x3000U) {
        AppendSeparator(output);
        return;
    }
    if (In(codepoint, 0xFF01U, 0xFF5EU)) {
        codepoint -= 0xFEE0U;
    }
    if (In(codepoint, 0x0300U, 0x036FU) || In(codepoint, 0xFE20U, 0xFE2FU)) {
        return;
    }
    if (AppendLatinFold(codepoint, output)) {
        return;
    }
    if (In(codepoint, static_cast<std::uint32_t>('A'), static_cast<std::uint32_t>('Z'))) {
        codepoint += static_cast<std::uint32_t>('a' - 'A');
    } else if (In(codepoint, 0x0391U, 0x03A1U) || In(codepoint, 0x03A3U, 0x03ABU)) {
        codepoint += 0x20U;
    } else if (codepoint == 0x03C2U) {
        codepoint = 0x03C3U;
    } else if (In(codepoint, 0x0410U, 0x042FU)) {
        codepoint += 0x20U;
    } else if (codepoint == 0x0401U) {
        codepoint = 0x0451U;
    }

    if (codepoint <= 0x7FU) {
        const auto character = static_cast<unsigned char>(codepoint);
        const bool alpha_numeric =
            (character >= static_cast<unsigned char>('a') &&
             character <= static_cast<unsigned char>('z')) ||
            (character >= static_cast<unsigned char>('0') &&
             character <= static_cast<unsigned char>('9'));
        if (alpha_numeric) {
            output.push_back(static_cast<char>(character));
        } else {
            AppendSeparator(output);
        }
        return;
    }

    if (In(codepoint, 0x2000U, 0x206FU) || In(codepoint, 0x2E00U, 0x2E7FU) ||
        In(codepoint, 0x3000U, 0x303FU)) {
        AppendSeparator(output);
        return;
    }
    AppendUtf8(codepoint, output);
}

void SplitTokens(const String& normalized, Vector<String>& tokens) {
    tokens.clear();
    std::size_t start = 0;
    while (start < normalized.size()) {
        const std::size_t end = normalized.find(' ', start);
        const std::size_t length =
            end == String::npos ? normalized.size() - start : end - start;
        if (length != 0) {
            tokens.emplace_back(normalized.substr(start, length));
        }
        if (end == String::npos) {
            break;
        }
        start = end + 1;
    }
}

bool StartsWith(const String& text, const String& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool TokensHavePrefixes(
    const Vector<String>& candidate_tokens,
    const Vector<String>& query_tokens) {
    if (query_tokens.empty()) {
        return false;
    }
    for (const auto& query_token : query_tokens) {
        const bool found = std::any_of(
            candidate_tokens.begin(),
            candidate_tokens.end(),
            [&query_token](const String& candidate_token) {
                return StartsWith(candidate_token, query_token);
            });
        if (!found) {
            return false;
        }
    }
    return true;
}

struct Match {
    MatchClass match_class;
    int field_priority;
};

template <typename IndexedApp>
bool Classify(
    const IndexedApp& app,
    const String& query,
    const Vector<String>& query_tokens,
    Match& match) {
    if (app.normalized_title == query || app.normalized_title_id == query) {
        match = Match{MatchClass::Exact, app.normalized_title == query ? 0 : 1};
        return true;
    }
    if (StartsWith(app.normalized_title, query) || StartsWith(app.normalized_title_id, query)) {
        match = Match{MatchClass::Prefix, StartsWith(app.normalized_title, query) ? 0 : 1};
        return true;
    }
    if (TokensHavePrefixes(app.title_tokens, query_tokens)) {
        match = Match{MatchClass::TokenPrefix, 0};
        return true;
    }
    if (app.normalized_title.find(query) != String::npos ||
        app.normalized_title_id.find(query) != String::npos) {
        match = Match{
            MatchClass::Substring,
            app.normalized_title.find(query) != String::npos ? 0 : 1};
        return true;
    }

    const bool metadata_match =
        app.normalized_name == query || app.normalized_category == query ||
        StartsWith(app.normalized_name, query) || StartsWith(app.normalized_category, query) ||
        TokensHavePrefixes(app.name_tokens, query_tokens) ||
        app.normalized_name.find(query) != String::npos ||
        app.normalized_category.find(query) != String::npos;
    if (metadata_match) {
        match = Match{MatchClass::Metadata, 0};
        return true;
    }
    return false;
}

} // namespace

void AppIndex::Assign(Vector<AppRecord>& apps) {
    apps_.clear();
    apps_.swap(apps);
    indexed_apps_.clear();
    indexed_apps_.reserve(apps_.size());
    for (std::size_t index = 0; index < apps_.size(); ++index) {
        const AppRecord& app = apps_[index];
        indexed_apps_.emplace_back();
        IndexedApp& indexed = indexed_apps_.back();
        indexed.app_index = index;
        indexed.normalized_title = NormalizeForSearch(app.display_title);
        indexed.normalized_title_id = NormalizeForSearch(app.title_id);
        indexed.normalized_name = NormalizeForSearch(app.app_name);
        indexed.normalized_category = NormalizeForSearch(app.category);
        SplitTokens(indexed.normalized_title, indexed.title_tokens);
        SplitTokens(indexed.normalized_name, indexed.name_tokens);
    }
}

void AppIndex::Swap(AppIndex& other) noexcept {
    apps_.swap(other.apps_);
    indexed_apps_.swap(other.indexed_apps_);
}

const Vector<AppRecord>& AppIndex::apps() const {
    return apps_;
}

void AppIndex::Search(
    const String& query,
    Vector<SearchResult>& results,
    std::size_t limit) const {
    results.clear();
    if (limit == 0) {
        return;
    }

    const String normalized_query = NormalizeForSearch(query);
    if (normalized_query.empty()) {
        return;
    }
    Vector<String> query_tokens;
    SplitTokens(normalized_query, query_tokens);

    struct Candidate {
        const IndexedApp* indexed;
        Match match;
    };

    Vector<Candidate> candidates;
    candidates.reserve(indexed_apps_.size());
    for (const auto& indexed : indexed_apps_) {
        const AppRecord& app = apps_[indexed.app_index];
        if (!app.launchable || app.title_id.empty()) {
            continue;
        }
        Match match;
        if (Classify(indexed, normalized_query, query_tokens, match)) {
            candidates.push_back(Candidate{&indexed, match});
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [this](const Candidate& left, const Candidate& right) {
            const AppRecord& left_app = apps_[left.indexed->app_index];
            const AppRecord& right_app = apps_[right.indexed->app_index];
            return std::tie(
                       left.match.match_class,
                       left.match.field_priority,
                       left.indexed->normalized_title,
                       left.indexed->normalized_title_id,
                       left_app.title_id) <
                   std::tie(
                       right.match.match_class,
                       right.match.field_priority,
                       right.indexed->normalized_title,
                       right.indexed->normalized_title_id,
                       right_app.title_id);
        });

    const std::size_t result_count = std::min(limit, candidates.size());
    results.reserve(result_count);
    for (std::size_t index = 0; index < result_count; ++index) {
        const Candidate& candidate = candidates[index];
        results.push_back(SearchResult{
            &apps_[candidate.indexed->app_index],
            candidate.match.match_class});
    }
}

void SortAndDeduplicateAppRecords(Vector<AppRecord>& records) {
    if (records.size() < 2) {
        return;
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const AppRecord& left, const AppRecord& right) {
            return left.title_id < right.title_id;
        });

    Vector<AppRecord> unique_records;
    unique_records.reserve(records.size());
    for (auto& record : records) {
        if (unique_records.empty() || unique_records.back().title_id != record.title_id) {
            unique_records.push_back(std::move(record));
        }
    }
    records.clear();
    records.swap(unique_records);
}

String NormalizeForSearch(const String& text) {
    String output;
    output.reserve(text.size());

    std::size_t offset = 0;
    while (offset < text.size()) {
        std::uint32_t codepoint = 0;
        if (!DecodeUtf8(text, offset, codepoint)) {
            AppendSeparator(output);
            continue;
        }
        AppendFolded(codepoint, output);
    }

    while (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }
    return output;
}

} // namespace livearea_search

#include <livearea_search/recent_apps.h>

#include <livearea_search/launch_uri.h>

namespace livearea_search {
namespace {

const char kHeader[] = "LIVEAREA_SEARCH_RECENTS_V1";

bool EqualBytes(
    const char* data,
    std::size_t length,
    const char* expected,
    std::size_t expected_length) {
    if (length != expected_length) {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        if (data[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

bool Contains(const Vector<String>& values, const String& value) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == value) {
            return true;
        }
    }
    return false;
}

bool IsAvailable(const Vector<AppRecord>& apps, const String& title_id) {
    for (std::size_t index = 0; index < apps.size(); ++index) {
        if (apps[index].launchable && apps[index].title_id == title_id) {
            return true;
        }
    }
    return false;
}

} // namespace

bool RecentApps::Parse(const char* data, std::size_t size) {
    Clear();
    if (data == NULL || size == 0 || size > kMaximumSerializedSize) {
        return false;
    }
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] == '\0' || data[index] == '\r') {
            return false;
        }
    }

    Vector<String> parsed;
    std::size_t entry_lines = 0;
    std::size_t line_start = 0;
    std::size_t line_number = 0;
    while (line_start < size) {
        std::size_t line_end = line_start;
        while (line_end < size && data[line_end] != '\n') {
            ++line_end;
        }
        const std::size_t length = line_end - line_start;
        if (line_number == 0) {
            if (!EqualBytes(data + line_start, length, kHeader, sizeof(kHeader) - 1)) {
                return false;
            }
        } else if (length != 0) {
            ++entry_lines;
            if (entry_lines > kCapacity) {
                return false;
            }
            const String title_id(data + line_start, length);
            if (!IsValidTitleId(title_id)) {
                return false;
            }
            if (!Contains(parsed, title_id)) {
                parsed.push_back(title_id);
            }
        }
        ++line_number;
        line_start = line_end == size ? size : line_end + 1;
    }

    title_ids_.swap(parsed);
    return line_number != 0;
}

void RecentApps::Clear() {
    title_ids_.clear();
}

void RecentApps::Swap(RecentApps& other) noexcept {
    title_ids_.swap(other.title_ids_);
}

void RecentApps::RecordSelection(const String& title_id) {
    if (!IsValidTitleId(title_id)) {
        return;
    }

    Vector<String> updated;
    updated.reserve(kCapacity);
    updated.push_back(title_id);
    for (std::size_t index = 0;
         index < title_ids_.size() && updated.size() < kCapacity;
         ++index) {
        if (title_ids_[index] != title_id) {
            updated.push_back(title_ids_[index]);
        }
    }
    title_ids_.swap(updated);
}

void RecentApps::RetainAvailable(const Vector<AppRecord>& apps) {
    Vector<String> retained;
    retained.reserve(title_ids_.size());
    for (std::size_t index = 0; index < title_ids_.size(); ++index) {
        if (IsAvailable(apps, title_ids_[index])) {
            retained.push_back(title_ids_[index]);
        }
    }
    title_ids_.swap(retained);
}

void RecentApps::Resolve(
    const Vector<AppRecord>& apps,
    Vector<SearchResult>& results) const {
    results.clear();
    results.reserve(title_ids_.size());
    for (std::size_t recent_index = 0;
         recent_index < title_ids_.size();
         ++recent_index) {
        for (std::size_t app_index = 0; app_index < apps.size(); ++app_index) {
            if (apps[app_index].launchable &&
                apps[app_index].title_id == title_ids_[recent_index]) {
                results.push_back(SearchResult(&apps[app_index], MatchClass::Metadata));
                break;
            }
        }
    }
}

String RecentApps::Serialize() const {
    String output(kHeader);
    output.push_back('\n');
    for (std::size_t index = 0;
         index < title_ids_.size() && index < kCapacity;
         ++index) {
        output.append(title_ids_[index]);
        output.push_back('\n');
    }
    return output;
}

const Vector<String>& RecentApps::title_ids() const {
    return title_ids_;
}

} // namespace livearea_search

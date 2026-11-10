#pragma once

#include <cstddef>

#include <livearea_search/app_index.h>

namespace livearea_search {

class RecentApps {
public:
    enum : std::size_t {
        kCapacity = 20,
        kMaximumSerializedSize = 1024,
    };

    bool Parse(const char* data, std::size_t size);
    void Clear();
    void RecordSelection(const String& title_id);
    void RetainAvailable(const Vector<AppRecord>& apps);
    void Resolve(
        const Vector<AppRecord>& apps,
        Vector<SearchResult>& results) const;
    String Serialize() const;

    const Vector<String>& title_ids() const;

private:
    Vector<String> title_ids_;
};

} // namespace livearea_search

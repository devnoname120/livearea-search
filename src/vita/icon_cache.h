#pragma once

#include <cstdint>

#include "paf_compat.h"

#include <livearea_search/types.h>

namespace livearea_search {

class IconCache {
public:
    static const std::uint32_t kReadyEvent = 0x1F510001U;

    explicit IconCache(paf::Plugin* plugin);
    ~IconCache();

    bool ready() const;
    bool Request(const String& key, const String& path);
    paf::intrusive_ptr<paf::graph::Surface> Get(const String& key);
    void Pin(const String& key);
    void Unpin(const String& key);
    void SetNotificationsEnabled(bool enabled);
    void ResetPins();
    void Clear();

private:
    class LoadJob : public paf::job::JobItem {
    public:
        LoadJob(IconCache* cache, const String& key, const String& path);

        void Run();
        void Finish();

    private:
        IconCache* cache_;
        String key_;
        String path_;
    };

    struct SurfaceEntry {
        SurfaceEntry(
            const String& key_value,
            const String& path_value)
            : key(key_value),
              path(path_value),
              last_used(0),
              pin_count(0),
              loading(false) {}

        String key;
        String path;
        paf::intrusive_ptr<paf::graph::Surface> surface;
        std::uint64_t last_used;
        std::size_t pin_count;
        bool loading;
    };

    bool Load(const String& key, const String& path);
    bool IsAlive();
    std::size_t FindEntry(const String& key) const;
    std::size_t SelectEvictionCandidate(std::size_t excluded_index) const;
    void MarkLoadFinished(const String& key);
    void Touch(SurfaceEntry& entry);

    paf::Plugin* plugin_;
    paf::thread::RMutex* mutex_;
    paf::job::JobQueue* queue_;
    Vector<SurfaceEntry> entries_;
    std::uint64_t use_clock_;
    bool notifications_enabled_;
    bool alive_;
};

} // namespace livearea_search

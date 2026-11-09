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
            const paf::intrusive_ptr<paf::graph::Surface>& surface_value)
            : key(key_value), surface(surface_value) {}

        String key;
        paf::intrusive_ptr<paf::graph::Surface> surface;
    };

    bool Load(const String& key, const String& path);
    bool IsAlive();
    void RemovePending(const String& key);
    bool HasSurface(const String& key) const;
    bool HasPending(const String& key) const;

    paf::Plugin* plugin_;
    paf::thread::RMutex* mutex_;
    paf::job::JobQueue* queue_;
    Vector<SurfaceEntry> surfaces_;
    Vector<String> pending_;
    std::size_t next_eviction_;
    bool alive_;
};

} // namespace livearea_search

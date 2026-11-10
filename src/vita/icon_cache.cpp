#include "icon_cache.h"

#include <psp2/io/fcntl.h>

#include <livearea_search/icon_cache_policy.h>

#include "paf_memory.h"
#include "widget_event.h"

namespace livearea_search {

namespace {

paf::intrusive_ptr<paf::graph::Surface> LoadSurface(
    const String& path,
    bool& source_opened) {
    source_opened = false;
    int error = -1;
    paf::common::SharedPtr<paf::LocalFile> file =
        paf::LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &error);
    if (error < 0 || file.get() == NULL) {
        return paf::intrusive_ptr<paf::graph::Surface>();
    }
    source_opened = true;
    paf::common::SharedPtr<paf::File> source =
        reinterpret_cast<paf::common::SharedPtr<paf::File>&>(file);
    return paf::graph::Surface::Load(
        paf::gutil::GetDefaultSurfacePool(),
        source);
}

} // namespace

IconCache::LoadJob::LoadJob(
    IconCache* cache,
    const String& key,
    const String& path)
    : paf::job::JobItem("LiveAreaSearch::IconLoad"),
      cache_(cache),
      key_(key),
      path_(path) {}

void IconCache::LoadJob::Run() {
    if (cache_ == NULL || !cache_->IsAlive()) {
        return;
    }
    cache_->Load(key_, path_);
}

void IconCache::LoadJob::Finish() {}

IconCache::IconCache(paf::Plugin* plugin)
    : plugin_(plugin),
      mutex_(PafNew<paf::thread::RMutex>("LiveAreaSearch::IconCache")),
      queue_(NULL),
      use_clock_(0),
      alive_(false) {
    if (mutex_ != NULL) {
        queue_ = PafNew<paf::job::JobQueue>("LiveAreaSearch::IconQueue");
    }
    alive_ = mutex_ != NULL && queue_ != NULL;
}

IconCache::~IconCache() {
    if (mutex_ != NULL) {
        mutex_->Lock();
        alive_ = false;
        mutex_->Unlock();
    }

    if (queue_ != NULL) {
        queue_->CancelAllItems();
        queue_->WaitEmpty();
    }
    if (mutex_ != NULL) {
        Clear();
    } else {
        entries_.clear();
    }
    delete queue_;
    delete mutex_;
}

bool IconCache::ready() const {
    return mutex_ != NULL && queue_ != NULL;
}

bool IconCache::Request(const String& key, const String& path) {
    if (!ready() || key.empty() || path.empty()) {
        return false;
    }

    mutex_->Lock();
    std::size_t entry_index = FindEntry(key);
    if (entry_index == kNoIconCacheEvictionCandidate) {
        entries_.push_back(SurfaceEntry(key, path));
        entry_index = entries_.size() - 1;
    }
    SurfaceEntry& entry = entries_[entry_index];
    entry.path = path;
    Touch(entry);
    const bool unavailable = !alive_ || entry.surface.get() != NULL || entry.loading;
    if (!unavailable) {
        entry.loading = true;
    }
    mutex_->Unlock();
    if (unavailable) {
        return false;
    }

    LoadJob* job = PafNew<LoadJob>(this, key, path);
    if (job == NULL) {
        MarkLoadFinished(key);
        return false;
    }
    paf::common::SharedPtr<paf::job::JobItem> item(job);
    const int result = queue_->Enqueue(item);
    if (result < 0) {
        MarkLoadFinished(key);
        return false;
    }
    return true;
}

paf::intrusive_ptr<paf::graph::Surface> IconCache::Get(const String& key) {
    if (mutex_ == NULL) {
        return paf::intrusive_ptr<paf::graph::Surface>();
    }
    mutex_->Lock();
    paf::intrusive_ptr<paf::graph::Surface> surface;
    const std::size_t entry_index = FindEntry(key);
    if (entry_index != kNoIconCacheEvictionCandidate) {
        SurfaceEntry& entry = entries_[entry_index];
        Touch(entry);
        surface = entry.surface;
    }
    mutex_->Unlock();
    return surface;
}

void IconCache::Pin(const String& key) {
    if (mutex_ == NULL) {
        return;
    }
    mutex_->Lock();
    const std::size_t entry_index = FindEntry(key);
    if (entry_index != kNoIconCacheEvictionCandidate) {
        SurfaceEntry& entry = entries_[entry_index];
        ++entry.pin_count;
        Touch(entry);
    }
    mutex_->Unlock();
}

void IconCache::Unpin(const String& key) {
    if (mutex_ == NULL) {
        return;
    }
    mutex_->Lock();
    const std::size_t entry_index = FindEntry(key);
    if (entry_index != kNoIconCacheEvictionCandidate) {
        SurfaceEntry& entry = entries_[entry_index];
        if (entry.pin_count != 0) {
            --entry.pin_count;
        }
        Touch(entry);
    }
    mutex_->Unlock();
}

void IconCache::Clear() {
    mutex_->Lock();
    entries_.clear();
    use_clock_ = 0;
    mutex_->Unlock();
}

bool IconCache::Load(const String& key, const String& path) {
    paf::intrusive_ptr<paf::graph::Surface> surface;
    bool source_opened = false;
    if (IsAlive()) {
        surface = LoadSurface(path, source_opened);
    }

    while (source_opened && surface.get() == NULL && IsAlive()) {
        paf::intrusive_ptr<paf::graph::Surface> evicted_surface;
        mutex_->Lock();
        const std::size_t entry_index = FindEntry(key);
        const std::size_t victim = SelectEvictionCandidate(entry_index);
        if (victim != kNoIconCacheEvictionCandidate) {
            evicted_surface = entries_[victim].surface;
            entries_[victim].surface.clear();
        }
        mutex_->Unlock();
        if (evicted_surface.get() == NULL) {
            break;
        }
        evicted_surface.clear();
        surface = LoadSurface(path, source_opened);
    }

    mutex_->Lock();
    const std::size_t entry_index = FindEntry(key);
    bool accepted = alive_ && surface.get() != NULL &&
                    entry_index != kNoIconCacheEvictionCandidate;
    if (entry_index != kNoIconCacheEvictionCandidate) {
        SurfaceEntry& entry = entries_[entry_index];
        entry.loading = false;
        Touch(entry);
        if (accepted && entry.surface.get() == NULL) {
            entry.surface = surface;
        } else if (entry.surface.get() != NULL) {
            accepted = false;
        }
    }
    mutex_->Unlock();

    if (accepted) {
        const std::uint32_t hash = paf::IDParam(key.c_str()).GetIDHash();
        paf::thread::RMutex::main_thread_mutex.Lock();
        BroadcastWidgetEvent(plugin_, kReadyEvent, static_cast<std::int32_t>(hash));
        paf::thread::RMutex::main_thread_mutex.Unlock();
    }
    return accepted;
}

bool IconCache::IsAlive() {
    if (mutex_ == NULL) {
        return false;
    }
    mutex_->Lock();
    const bool alive = alive_;
    mutex_->Unlock();
    return alive;
}

std::size_t IconCache::FindEntry(const String& key) const {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        if (entries_[index].key == key) {
            return index;
        }
    }
    return kNoIconCacheEvictionCandidate;
}

std::size_t IconCache::SelectEvictionCandidate(std::size_t excluded_index) const {
    std::size_t selected = kNoIconCacheEvictionCandidate;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const SurfaceEntry& entry = entries_[index];
        const IconCacheEvictionEntry current = {
            entry.surface.get() != NULL,
            entry.loading,
            entry.surface.get() != NULL && entry.surface->GetRefCount() == 1,
            entry.pin_count,
            entry.last_used};
        IconCacheEvictionEntry candidates[2] = {
            {false, false, false, 0, 0},
            current,
        };
        if (selected != kNoIconCacheEvictionCandidate) {
            const SurfaceEntry& previous = entries_[selected];
            candidates[0] = IconCacheEvictionEntry{
                previous.surface.get() != NULL,
                previous.loading,
                previous.surface.get() != NULL && previous.surface->GetRefCount() == 1,
                previous.pin_count,
                previous.last_used};
        }
        const std::size_t local_excluded =
            index == excluded_index ? 1 : kNoIconCacheEvictionCandidate;
        const std::size_t local = SelectIconCacheEvictionCandidate(
            candidates,
            2,
            local_excluded);
        if (local == 1) {
            selected = index;
        }
    }
    return selected;
}

void IconCache::MarkLoadFinished(const String& key) {
    mutex_->Lock();
    const std::size_t entry_index = FindEntry(key);
    if (entry_index != kNoIconCacheEvictionCandidate) {
        entries_[entry_index].loading = false;
    }
    mutex_->Unlock();
}

void IconCache::Touch(SurfaceEntry& entry) {
    ++use_clock_;
    entry.last_used = use_clock_;
}

} // namespace livearea_search

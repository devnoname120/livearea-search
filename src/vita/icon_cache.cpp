#include "icon_cache.h"

#include <psp2/io/fcntl.h>

#include "paf_memory.h"
#include "widget_event.h"

namespace livearea_search {

namespace {

// Bound GPU-backed surfaces in the resident shell while visible rows retain their own references.
const std::size_t kMaxCachedSurfaces = 64;

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
      next_eviction_(0),
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
        surfaces_.clear();
        pending_.clear();
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
    const bool unavailable = !alive_ || HasSurface(key) || HasPending(key);
    if (!unavailable) {
        pending_.push_back(key);
    }
    mutex_->Unlock();
    if (unavailable) {
        return false;
    }

    LoadJob* job = PafNew<LoadJob>(this, key, path);
    if (job == NULL) {
        RemovePending(key);
        return false;
    }
    paf::common::SharedPtr<paf::job::JobItem> item(job);
    const int result = queue_->Enqueue(item);
    if (result < 0) {
        RemovePending(key);
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
    for (std::size_t index = 0; index < surfaces_.size(); ++index) {
        if (surfaces_[index].key == key) {
            surface = surfaces_[index].surface;
            break;
        }
    }
    mutex_->Unlock();
    return surface;
}

void IconCache::Clear() {
    mutex_->Lock();
    surfaces_.clear();
    pending_.clear();
    next_eviction_ = 0;
    mutex_->Unlock();
}

bool IconCache::Load(const String& key, const String& path) {
    int error = -1;
    paf::common::SharedPtr<paf::LocalFile> file =
        paf::LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &error);
    paf::intrusive_ptr<paf::graph::Surface> surface;
    if (error >= 0 && file.get() != NULL && IsAlive()) {
        paf::common::SharedPtr<paf::File> source =
            reinterpret_cast<paf::common::SharedPtr<paf::File>&>(file);
        surface = paf::graph::Surface::Load(
            paf::gutil::GetDefaultSurfacePool(),
            source);
    }

    mutex_->Lock();
    for (std::size_t index = 0; index < pending_.size(); ++index) {
        if (pending_[index] == key) {
            pending_[index] = pending_.back();
            pending_.pop_back();
            break;
        }
    }
    const bool accepted = alive_ && surface.get() != NULL;
    if (accepted) {
        if (surfaces_.size() < kMaxCachedSurfaces) {
            surfaces_.push_back(SurfaceEntry(key, surface));
        } else {
            surfaces_[next_eviction_] = SurfaceEntry(key, surface);
            next_eviction_ = (next_eviction_ + 1) % kMaxCachedSurfaces;
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

void IconCache::RemovePending(const String& key) {
    mutex_->Lock();
    for (std::size_t index = 0; index < pending_.size(); ++index) {
        if (pending_[index] == key) {
            pending_[index] = pending_.back();
            pending_.pop_back();
            break;
        }
    }
    mutex_->Unlock();
}

bool IconCache::HasSurface(const String& key) const {
    for (std::size_t index = 0; index < surfaces_.size(); ++index) {
        if (surfaces_[index].key == key) {
            return true;
        }
    }
    return false;
}

bool IconCache::HasPending(const String& key) const {
    for (std::size_t index = 0; index < pending_.size(); ++index) {
        if (pending_[index] == key) {
            return true;
        }
    }
    return false;
}

} // namespace livearea_search

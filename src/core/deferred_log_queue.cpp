#include <livearea_search/deferred_log_queue.h>

namespace livearea_search {

DeferredLogQueue::DeferredLogQueue() noexcept : head_(0), count_(0) {}

void DeferredLogQueue::Clear() noexcept {
    head_ = 0;
    count_ = 0;
}

void DeferredLogQueue::Push(const char* message) noexcept {
    if (message == nullptr) {
        message = "<null>";
    }

    if (count_ == kCapacity) {
        head_ = (head_ + 1) % kCapacity;
        --count_;
    }

    const std::size_t index = (head_ + count_) % kCapacity;
    std::size_t length = 0;
    while (message[length] != '\0' && length + 1 < kLineCapacity) {
        lines_[index][length] = message[length];
        ++length;
    }
    lines_[index][length] = '\0';
    ++count_;
}

bool DeferredLogQueue::Pop(char* destination, std::size_t capacity) noexcept {
    if (count_ == 0 || destination == nullptr || capacity == 0) {
        return false;
    }

    std::size_t length = 0;
    while (lines_[head_][length] != '\0' && length + 1 < capacity) {
        destination[length] = lines_[head_][length];
        ++length;
    }
    destination[length] = '\0';

    head_ = (head_ + 1) % kCapacity;
    --count_;
    return true;
}

std::size_t DeferredLogQueue::size() const noexcept {
    return count_;
}

} // namespace livearea_search

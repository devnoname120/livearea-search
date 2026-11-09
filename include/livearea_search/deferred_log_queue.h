#pragma once

#include <cstddef>

namespace livearea_search {

class DeferredLogQueue {
public:
    static const std::size_t kCapacity = 16;
    static const std::size_t kLineCapacity = 128;

    DeferredLogQueue() noexcept;

    void Clear() noexcept;
    void Push(const char* message) noexcept;
    bool Pop(char* destination, std::size_t capacity) noexcept;
    std::size_t size() const noexcept;

private:
    char lines_[kCapacity][kLineCapacity];
    std::size_t head_;
    std::size_t count_;
};

} // namespace livearea_search

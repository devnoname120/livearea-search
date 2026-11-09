#pragma once

#include <cstddef>

#if defined(LIVEAREA_SEARCH_PAF_TYPES)
#include <cstdarg>
#include <limits>

#include <paf.h>
#else
#include <string>
#include <vector>
#endif

namespace livearea_search {

#if defined(LIVEAREA_SEARCH_PAF_TYPES)

class String {
public:
    static const std::size_t npos = static_cast<std::size_t>(-1);

    String() {}

    String(const char* text)
        : value_(text == NULL ? "" : text) {}

    String(const char* text, std::size_t length)
        : value_(text == NULL ? "" : text, text == NULL ? 0 : length) {}

    String(const String& other)
        : value_(other.value_) {}

    String(String&& other)
        : value_(other.value_) {
        other.clear();
    }

    String& operator=(const String& other) {
        value_ = other.value_;
        return *this;
    }

    String& operator=(String&& other) {
        if (this != &other) {
            value_ = other.value_;
            other.clear();
        }
        return *this;
    }

    String& operator=(const char* text) {
        value_ = text == NULL ? "" : text;
        return *this;
    }

    const char* c_str() const {
        return value_.c_str();
    }

    const char* data() const {
        return value_.data();
    }

    char* data() {
        return value_.data();
    }

    std::size_t size() const {
        return value_.size();
    }

    bool empty() const {
        return value_.empty();
    }

    void clear() {
        value_.clear();
    }

    void reserve(std::size_t) {}

    String& assign(const char* text, std::size_t length) {
        value_.assign(text, length);
        return *this;
    }

    String& append(const char* text, std::size_t length) {
        value_.append(text, length);
        return *this;
    }

    String& append(const char* text) {
        value_.append(text == NULL ? "" : text);
        return *this;
    }

    String& append(const String& text) {
        value_.append(text.value_);
        return *this;
    }

    String& operator+=(const char* text) {
        return append(text);
    }

    String& operator+=(const String& text) {
        return append(text);
    }

    void push_back(char value) {
        value_.append(1, value);
    }

    void pop_back() {
        if (!empty()) {
            value_.erase(size() - 1, 1);
        }
    }

    char& operator[](std::size_t index) {
        return value_.data()[index];
    }

    const char& operator[](std::size_t index) const {
        return value_.data()[index];
    }

    char& back() {
        return value_.data()[size() - 1];
    }

    const char& back() const {
        return value_.data()[size() - 1];
    }

    String substr(std::size_t position, std::size_t length = npos) const {
        if (position >= size()) {
            return String();
        }
        const std::size_t available = size() - position;
        const std::size_t actual = length == npos || length > available ? available : length;
        return String(data() + position, actual);
    }

    std::size_t find(char value, std::size_t position = 0) const {
        for (std::size_t index = position; index < size(); ++index) {
            if ((*this)[index] == value) {
                return index;
            }
        }
        return npos;
    }

    std::size_t find(const String& needle, std::size_t position = 0) const {
        return find(needle.data(), needle.size(), position);
    }

    std::size_t find(const char* needle, std::size_t position = 0) const {
        if (needle == NULL) {
            return npos;
        }
        std::size_t length = 0;
        while (needle[length] != '\0') {
            ++length;
        }
        return find(needle, length, position);
    }

    int compare(std::size_t position, std::size_t length, const String& other) const {
        const std::size_t available = position < size() ? size() - position : 0;
        const std::size_t left_length = length < available ? length : available;
        const std::size_t common = left_length < other.size() ? left_length : other.size();
        for (std::size_t index = 0; index < common; ++index) {
            const unsigned char left = static_cast<unsigned char>((*this)[position + index]);
            const unsigned char right = static_cast<unsigned char>(other[index]);
            if (left != right) {
                return left < right ? -1 : 1;
            }
        }
        if (left_length == other.size()) {
            return 0;
        }
        return left_length < other.size() ? -1 : 1;
    }

    bool operator==(const String& other) const {
        return size() == other.size() && compare(0, size(), other) == 0;
    }

    bool operator!=(const String& other) const {
        return !(*this == other);
    }

    bool operator<(const String& other) const {
        return compare(0, size(), other) < 0;
    }

private:
    std::size_t find(const char* needle, std::size_t length, std::size_t position) const {
        if (length == 0) {
            return position <= size() ? position : npos;
        }
        if (position >= size() || length > size() - position) {
            return npos;
        }
        const std::size_t last = size() - length;
        for (std::size_t start = position; start <= last; ++start) {
            std::size_t index = 0;
            while (index < length && (*this)[start + index] == needle[index]) {
                ++index;
            }
            if (index == length) {
                return start;
            }
        }
        return npos;
    }

    paf::string value_;
};

template <typename T>
using Vector = paf::vector<T>;

#else

using String = std::string;

template <typename T>
using Vector = std::vector<T>;

#endif

} // namespace livearea_search

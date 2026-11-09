#include <livearea_search/config_failsafe.h>

namespace livearea_search {
namespace {

const char kCanonicalPluginPath[] = "ur0:tai/livearea_search.suprx";
const char kSlashPluginPath[] = "ur0:/tai/livearea_search.suprx";

bool IsLineBreak(char value) {
    return value == '\n' || value == '\r';
}

bool IsHorizontalSpace(char value) {
    return value == ' ' || value == '\t';
}

bool HasPathAt(
    const char* contents,
    std::size_t line_end,
    std::size_t path_start,
    const char* path,
    std::size_t path_size) {
    if (path_start > line_end || path_size > line_end - path_start) {
        return false;
    }
    for (std::size_t index = 0; index < path_size; ++index) {
        if (contents[path_start + index] != path[index]) {
            return false;
        }
    }

    const std::size_t path_end = path_start + path_size;
    for (std::size_t index = path_end; index < line_end; ++index) {
        if (!IsHorizontalSpace(contents[index])) {
            return false;
        }
    }
    return true;
}

bool IsPluginPathAt(
    const char* contents,
    std::size_t line_end,
    std::size_t path_start) {
    return HasPathAt(
               contents,
               line_end,
               path_start,
               kCanonicalPluginPath,
               sizeof(kCanonicalPluginPath) - 1) ||
           HasPathAt(
               contents,
               line_end,
               path_start,
               kSlashPluginPath,
               sizeof(kSlashPluginPath) - 1);
}

} // namespace

std::size_t FindConfigSelfDisableOffset(
    const char* contents,
    std::size_t size,
    std::size_t search_from) {
    if (contents == nullptr || search_from >= size) {
        return kConfigEntryNotFound;
    }

    std::size_t line_start = 0;
    while (line_start < size) {
        std::size_t line_end = line_start;
        while (line_end < size && !IsLineBreak(contents[line_end])) {
            ++line_end;
        }

        if (line_start >= search_from) {
            std::size_t path_start = line_start;
            while (path_start < line_end && IsHorizontalSpace(contents[path_start])) {
                ++path_start;
            }
            if (IsPluginPathAt(contents, line_end, path_start)) {
                return line_start;
            }
        }

        if (line_end == size) {
            break;
        }
        line_start = line_end + 1;
        if (contents[line_end] == '\r' && line_start < size &&
            contents[line_start] == '\n') {
            ++line_start;
        }
    }

    return kConfigEntryNotFound;
}

std::size_t CountConfigSelfDisableEntries(
    const char* contents,
    std::size_t size) {
    std::size_t count = 0;
    std::size_t search_from = 0;
    while (true) {
        const std::size_t offset =
            FindConfigSelfDisableOffset(contents, size, search_from);
        if (offset == kConfigEntryNotFound) {
            return count;
        }
        ++count;
        search_from = offset + 1;
    }
}

} // namespace livearea_search

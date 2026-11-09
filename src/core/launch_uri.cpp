#include <livearea_search/launch_uri.h>

namespace livearea_search {

bool IsValidTitleId(const String& title_id) {
    if (title_id.empty() || title_id.size() > kMaxTitleIdLength) {
        return false;
    }
    for (std::size_t index = 0; index < title_id.size(); ++index) {
        const char value = title_id[index];
        const auto character = static_cast<unsigned char>(value);
        const bool allowed =
            (character >= static_cast<unsigned char>('A') &&
             character <= static_cast<unsigned char>('Z')) ||
            (character >= static_cast<unsigned char>('a') &&
             character <= static_cast<unsigned char>('z')) ||
            (character >= static_cast<unsigned char>('0') &&
             character <= static_cast<unsigned char>('9')) ||
            character == static_cast<unsigned char>('-') ||
            character == static_cast<unsigned char>('_') ||
            character == static_cast<unsigned char>('.');
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool BuildOpenLiveAreaUri(const String& title_id, String& uri) {
    if (!IsValidTitleId(title_id)) {
        uri.clear();
        return false;
    }
    static const char prefix[] = "psgm:open?titleid=";
    uri.clear();
    uri.reserve(sizeof(prefix) - 1 + title_id.size());
    uri.append(prefix);
    uri.append(title_id);
    return true;
}

} // namespace livearea_search

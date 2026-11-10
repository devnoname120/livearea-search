#pragma once

#include <livearea_search/recent_apps.h>

namespace livearea_search {

class RecentAppsFile {
public:
    int Load(RecentApps& recent_apps) const;
    int Save(const RecentApps& recent_apps) const;
};

} // namespace livearea_search

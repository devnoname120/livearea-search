#pragma once

#include "paf_compat.h"

namespace livearea_search {

class NotificationCornerGuard {
public:
    explicit NotificationCornerGuard(paf::Plugin* plugin);
    ~NotificationCornerGuard();

    bool Suppress();
    bool Restore();

private:
    paf::Plugin* plugin_;
    paf::ui::Scene* page_;
};

} // namespace livearea_search

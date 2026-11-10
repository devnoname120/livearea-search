#pragma once

#include "paf_compat.h"

namespace livearea_search {

class NotificationCornerGuard {
public:
    NotificationCornerGuard();
    ~NotificationCornerGuard();

    bool Suppress();
    bool Restore();

private:
    paf::ui::Widget* FindWidget() const;

    bool suppression_started_;
    bool was_visible_;
};

} // namespace livearea_search

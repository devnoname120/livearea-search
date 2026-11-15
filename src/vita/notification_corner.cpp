#include "notification_corner.h"

#include <livearea_search/overlay_visibility.h>

namespace livearea_search {

NotificationCornerGuard::NotificationCornerGuard()
    : suppressed_(false),
      was_visible_(false) {}

NotificationCornerGuard::~NotificationCornerGuard() {
    Restore();
}

paf::ui::Widget* NotificationCornerGuard::ResolveNativeCorner() const {
    paf::Plugin* indicator = paf::Plugin::Find("indicator_plugin");
    paf::ui::Scene* root = indicator == NULL ? NULL :
        indicator->PageRoot(kIndicatorPageHash);
    return root == NULL ? NULL :
        root->FindChild(kIndicatorNotificationBubbleHash);
}

bool NotificationCornerGuard::Suppress() {
    if (suppressed_) {
        return true;
    }

    paf::ui::Widget* native_corner = ResolveNativeCorner();
    if (native_corner == NULL) {
        return false;
    }
    was_visible_ = native_corner->IsVisible();
    suppressed_ = true;
    native_corner->Hide(paf::common::transition::Type_Reset);
    return true;
}

bool NotificationCornerGuard::Restore() {
    if (!suppressed_) {
        return true;
    }

    paf::ui::Widget* native_corner = ResolveNativeCorner();
    const bool should_show = was_visible_;
    suppressed_ = false;
    was_visible_ = false;
    if (native_corner == NULL) {
        return true;
    }
    if (should_show) {
        native_corner->Show(paf::common::transition::Type_Reset);
    }
    return true;
}

} // namespace livearea_search

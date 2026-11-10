#include "notification_corner.h"

namespace livearea_search {

NotificationCornerGuard::NotificationCornerGuard(paf::Plugin* plugin)
    : plugin_(plugin),
      page_(NULL) {}

NotificationCornerGuard::~NotificationCornerGuard() {
    Restore();
}

bool NotificationCornerGuard::Suppress() {
    if (page_ != NULL) {
        return true;
    }
    if (plugin_ == NULL) {
        return false;
    }

    paf::Plugin::PageOpenParam occluder_param;
    occluder_param.fade = false;
    occluder_param.overwrite_draw_priority = 7;
    page_ = plugin_->PageOpen("page_notification_occluder", occluder_param);
    return page_ != NULL;
}

bool NotificationCornerGuard::Restore() {
    if (page_ == NULL) {
        return true;
    }
    if (plugin_ == NULL) {
        return false;
    }

    paf::Plugin::PageCloseParam close_param;
    close_param.fade = false;
    plugin_->PageClose("page_notification_occluder", close_param);
    page_ = NULL;
    return true;
}

} // namespace livearea_search

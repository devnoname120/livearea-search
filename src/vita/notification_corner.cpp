#include "notification_corner.h"

#include <cstdint>

namespace livearea_search {
namespace {

const std::uint32_t kNotificationCornerWidgetHash = 0x718CB7FFU;

} // namespace

NotificationCornerGuard::NotificationCornerGuard()
    : suppression_started_(false),
      was_visible_(false) {}

NotificationCornerGuard::~NotificationCornerGuard() {
    Restore();
}

bool NotificationCornerGuard::Suppress() {
    paf::ui::Widget* const widget = FindWidget();
    if (widget == NULL) {
        return false;
    }
    if (!suppression_started_) {
        was_visible_ = widget->IsVisible();
        suppression_started_ = true;
    }
    widget->Hide(paf::common::transition::Type_Reset);
    return true;
}

bool NotificationCornerGuard::Restore() {
    if (!suppression_started_) {
        return true;
    }
    paf::ui::Widget* const widget = FindWidget();
    if (widget == NULL) {
        return false;
    }
    if (was_visible_) {
        widget->Show(paf::common::transition::Type_Reset);
    } else {
        widget->Hide(paf::common::transition::Type_Reset);
    }
    suppression_started_ = false;
    was_visible_ = false;
    return true;
}

paf::ui::Widget* NotificationCornerGuard::FindWidget() const {
    paf::Plugin* const indicator = paf::Plugin::Find("indicator_plugin");
    if (indicator == NULL || indicator->GetResource() == NULL) {
        return NULL;
    }
    paf::cxml::Element page = indicator->GetResource()
        ->GetTableElement(paf::resource::TableType_Page)
        .GetFirstChild();
    while (page == true) {
        std::uint32_t page_id = 0;
        if (paf::cxml::util::GetIDHash(page, "id", &page_id) >= 0) {
            paf::ui::Scene* const root = indicator->PageRoot(page_id);
            if (root != NULL) {
                paf::ui::Widget* const widget =
                    root->FindChild(paf::IDParam(kNotificationCornerWidgetHash));
                if (widget != NULL) {
                    return widget;
                }
            }
        }
        page = page.GetNextSibling();
    }
    return NULL;
}

} // namespace livearea_search

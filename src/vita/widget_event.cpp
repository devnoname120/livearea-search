#include "widget_event.h"

#include "paf_compat.h"

namespace livearea_search {

namespace {

void DispatchRecursive(paf::ui::Widget* widget, std::uint32_t type, paf::ui::Event* event) {
    if (widget == NULL) {
        return;
    }
    const int child_count = widget->GetChildrenNum();
    for (int index = 0; index < child_count; ++index) {
        DispatchRecursive(widget->GetChild(index), type, event);
    }
    widget->DoEvent(type, event);
}

} // namespace

void BroadcastWidgetEvent(
    paf::Plugin* plugin,
    std::uint32_t type,
    std::int32_t value0,
    std::int32_t value1) {
    if (plugin == NULL || plugin->GetResource() == NULL) {
        return;
    }

    paf::ui::Event event(
        paf::ui::EV_COMMAND,
        paf::ui::Event::MODE_DISPATCH,
        paf::ui::Event::ROUTE_FORWARD,
        0,
        value0,
        value1,
        0,
        0);
    paf::cxml::Element page =
        plugin->GetResource()->GetTableElement(paf::resource::TableType_Page).GetFirstChild();
    while (page == true) {
        std::uint32_t page_id = 0;
        paf::cxml::util::GetIDHash(page, "id", &page_id);
        DispatchRecursive(plugin->PageRoot(page_id), type, &event);
        page = page.GetNextSibling();
    }
}

} // namespace livearea_search

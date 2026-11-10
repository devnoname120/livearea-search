#pragma once

#include "paf_compat.h"

namespace livearea_search {
namespace paf_page_order {

// ScePaf exports these methods, but the reconstructed public header marks them private.
extern "C" void SetDrawOrderRemove(
    paf::Plugin* plugin,
    paf::ui::Scene* page)
    __asm__("_ZN3paf6Plugin19SetDrawOrder_RemoveEPNS_2ui5SceneE");

extern "C" void AdjustPageDrawOrder(
    paf::Plugin* plugin,
    paf::ui::Scene* page,
    const paf::IDParam& id,
    int priority)
    __asm__("_ZN3paf6Plugin19AdjustPageDrawOrderEPNS_2ui5SceneERKNS_7IDParamEi");

inline void SetPagePriority(
    paf::Plugin* plugin,
    paf::ui::Scene* page,
    const char* page_id,
    int priority) {
    if (plugin == NULL || page == NULL || page_id == NULL) {
        return;
    }

    const paf::IDParam id(page_id);
    SetDrawOrderRemove(plugin, page);
    AdjustPageDrawOrder(plugin, page, id, priority);
}

} // namespace paf_page_order
} // namespace livearea_search

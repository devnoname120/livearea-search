#include "launcher.h"

#if defined(LIVEAREA_SEARCH_VITASDK)
#include <psp2/appmgr.h>
#else
#include <appmgr.h>
#endif

#include <livearea_search/launch_uri.h>

#include "shell_state.h"

namespace livearea_search {

int OpenTitleLiveArea(const String& title_id) {
    String uri;
    if (!BuildOpenLiveAreaUri(title_id, uri)) {
        return -1;
    }
    SuppressOverlayFor(1500000ULL);
    return sceAppMgrLaunchAppByUri(0x20000, uri.c_str());
}

} // namespace livearea_search

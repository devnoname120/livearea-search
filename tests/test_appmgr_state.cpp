#include <livearea_search/appmgr_state.h>

#include <cstddef>
#include <cstdio>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

} // namespace

int main() {
    using livearea_search::AppMgrAppStateLayout;
    using livearea_search::ShouldAllowShellOverlay;

    Check(sizeof(AppMgrAppStateLayout) == 0x80,
          "AppMgr app-state layout must remain 128 bytes");
    Check(offsetof(AppMgrAppStateLayout, is_system_ui_overlaid) == 0x08,
          "AppMgr system-UI overlay word must remain at offset 0x08");

    Check(ShouldAllowShellOverlay(true, false, false),
          "plain Home with no system overlay must allow Search");
    Check(!ShouldAllowShellOverlay(true, true, false),
          "system UI overlay must suppress Search");
    Check(!ShouldAllowShellOverlay(false, false, false),
          "foreground application ownership must suppress Search");
    Check(!ShouldAllowShellOverlay(true, false, true),
          "explicit suppression window must suppress Search");

    return failures == 0 ? 0 : 1;
}

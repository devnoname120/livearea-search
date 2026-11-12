#include <livearea_search/overlay_visibility.h>

#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    using livearea_search::ComputeHomePadCloseAction;
    using livearea_search::HomePadCloseAction;

    const std::uint32_t ps = 0x00001000U;

    std::uint32_t current = ps | 0x20U;
    bool owned = false;
    Check(
        livearea_search::ObserveHomePadPress(true, ps, current, owned),
        "an owned PS rising edge requests Search closure");
    Check(current == 0x20U && owned,
          "the closing PS press is removed from the forwarded PAF sample");

    current = ps | 0x40U;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "a held PS button does not request closure repeatedly");
    Check(current == 0x40U && owned,
          "an owned held PS press remains hidden while unrelated buttons survive");
    Check(
        ComputeHomePadCloseAction(true, owned) ==
            HomePadCloseAction::WaitForRelease,
        "Search must remain visible while the owned PS press is physically down");

    current = 0U;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "releasing an owned PS press does not request closure");
    Check(!owned, "releasing PS ends ownership");
    Check(
        ComputeHomePadCloseAction(true, owned) ==
            HomePadCloseAction::ReleaseLockAndClose,
        "button-up must unlock SceShell before Search exposes Home");
    Check(
        ComputeHomePadCloseAction(false, owned) == HomePadCloseAction::None,
        "an idle Home-close policy must not release the shell lock");

    current = ps;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "PS is not intercepted while Search is closed");
    Check(current == ps && !owned,
          "closed Search leaves the shell pad state untouched");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    return 0;
}

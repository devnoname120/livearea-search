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
    using livearea_search::ComputeHomeEditLockAction;
    using livearea_search::ComputeHomePadCloseAction;
    using livearea_search::HomeEditLockAction;
    using livearea_search::HomePadCloseAction;
    using livearea_search::ObserveSearchOwnedButton;

    const std::uint32_t ps = 0x00001000U;
    const std::uint32_t triangle = 0x00000040U;

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

    bool triangle_owned = false;
    current = triangle;
    Check(
        !ObserveSearchOwnedButton(
            false,
            triangle,
            current,
            triangle_owned),
        "IME-active Search must not claim Triangle from the keyboard");
    Check(current == triangle && !triangle_owned,
          "IME-active Search forwards Triangle unchanged");

    current = triangle | 0x80U;
    Check(
        ObserveSearchOwnedButton(
            true,
            triangle,
            current,
            triangle_owned),
        "Search claims a Triangle rising edge while results are open");
    Check(current == 0x80U && triangle_owned,
          "owned Triangle is removed while unrelated buttons survive");

    current = triangle | 0x10U;
    Check(
        !ObserveSearchOwnedButton(
            true,
            triangle,
            current,
            triangle_owned),
        "a held or repeated Triangle press is not claimed twice");
    Check(current == 0x10U && triangle_owned,
          "held Triangle remains hidden from the previous Home handler");

    current = triangle | 0x20U;
    Check(
        !ObserveSearchOwnedButton(
            false,
            triangle,
            current,
            triangle_owned),
        "closing Search while Triangle is held does not create another claim");
    Check(current == 0x20U && triangle_owned,
          "Triangle ownership survives Search closure until physical release");

    current = 0x20U;
    Check(
        !ObserveSearchOwnedButton(
            false,
            triangle,
            current,
            triangle_owned),
        "Triangle button-up does not claim an action");
    Check(current == 0x20U && !triangle_owned,
          "Triangle button-up releases ownership without altering other input");

    current = triangle;
    Check(
        !ObserveSearchOwnedButton(
            false,
            triangle,
            current,
            triangle_owned),
        "Triangle is not intercepted while Search is closed");
    Check(current == triangle && !triangle_owned,
          "closed Search forwards a fresh Triangle press to SceShell");

    Check(
        ComputeHomeEditLockAction(false, false, false, false) ==
            HomeEditLockAction::None,
        "an inactive Search surface does not acquire the TopMenu edit lock");
    Check(
        ComputeHomeEditLockAction(false, true, false, false) ==
            HomeEditLockAction::Acquire,
        "keyboard-dismissed Search acquires the TopMenu edit lock");
    Check(
        ComputeHomeEditLockAction(true, true, false, false) ==
            HomeEditLockAction::None,
        "an already-held TopMenu edit lock is stable");
    Check(
        ComputeHomeEditLockAction(true, false, true, false) ==
            HomeEditLockAction::None,
        "unlock is deferred while an owned Triangle press remains down");
    Check(
        ComputeHomeEditLockAction(true, false, false, false) ==
            HomeEditLockAction::Release,
        "button-up releases a no-longer-needed TopMenu edit lock");
    Check(
        ComputeHomeEditLockAction(true, false, true, true) ==
            HomeEditLockAction::Release,
        "teardown force-releases the TopMenu edit lock even during a held press");
    Check(
        ComputeHomeEditLockAction(false, true, true, false) ==
            HomeEditLockAction::Acquire,
        "a stale local Triangle owner cannot prevent a required lock acquisition");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    return 0;
}

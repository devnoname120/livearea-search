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
    const std::uint32_t ps = 0x00001000U;

    std::uint32_t current = ps | 0x20U;
    bool owned = false;
    Check(
        livearea_search::ObserveHomePadPress(true, ps, current, owned),
        "an owned PS rising edge requests Search closure");
    Check(current == (ps | 0x20U) && owned,
          "observing PS leaves the PAF sample unchanged");

    current = ps;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "a held PS button does not request closure repeatedly");
    Check(current == ps && owned,
          "an owned PS press remains visible until physical release");

    current = 0U;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "releasing an owned PS press does not request closure");
    Check(!owned, "releasing PS ends ownership");

    current = ps;
    Check(!livearea_search::ObserveHomePadPress(false, ps, current, owned),
          "PS is not intercepted while Search is closed");
    Check(current == ps && !owned,
          "closed Search leaves the shell pad state untouched");

    Check(!livearea_search::ShouldReleaseHomePadLock(false, false, false),
          "an unheld shell lock is never released");
    Check(!livearea_search::ShouldReleaseHomePadLock(true, true, false),
          "the shell lock remains held while Search is open");
    Check(!livearea_search::ShouldReleaseHomePadLock(true, false, true),
          "Home closure keeps the shell lock through physical release");
    Check(livearea_search::ShouldReleaseHomePadLock(true, false, false),
          "closed Search releases the shell lock after PS release");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    return 0;
}

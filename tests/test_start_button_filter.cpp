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
    const std::uint32_t start = 0x00000200U;

    std::uint32_t current = start | 0x40U;
    bool owned = false;
    Check(
        !livearea_search::ObserveStartButtonPress(
            false,
            start,
            current,
            owned),
        "hidden Search must not open from Start");
    Check(current == (start | 0x40U) && !owned,
          "hidden Search must forward Start unchanged");

    current = start | 0x40U;
    Check(
        livearea_search::ObserveStartButtonPress(
            true,
            start,
            current,
            owned),
        "visible Search must open on a Start rising edge");
    Check(current == 0x40U && owned,
          "an owned Start press must be removed from the forwarded sample");

    current = start | 0x80U;
    Check(
        !livearea_search::ObserveStartButtonPress(
            false,
            start,
            current,
            owned),
        "a held Start press must not request Search repeatedly");
    Check(current == 0x80U && owned,
          "an owned held Start press must stay hidden while unrelated buttons survive");

    current = 0U;
    Check(
        !livearea_search::ObserveStartButtonPress(
            false,
            start,
            current,
            owned),
        "Start release must not request Search");
    Check(!owned, "Start release must end ownership");

    current = start;
    Check(
        livearea_search::ObserveStartButtonPress(
            true,
            start,
            current,
            owned),
        "a later visible Start press must open Search again");
    Check(current == 0U && owned,
          "the later owned press must also be removed from the forwarded sample");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    return 0;
}

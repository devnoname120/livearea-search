#include <livearea_search/overlay_visibility.h>

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
    using livearea_search::ShouldExposeHomeSearch;

    Check(
        ShouldExposeHomeSearch(true, true, false, false),
        "bubble Home must expose Search when no higher shell surface is visible");
    Check(
        !ShouldExposeHomeSearch(true, true, false, true),
        "a visible LiveArea card must own the foreground and hide Search");

    return failures == 0 ? 0 : 1;
}

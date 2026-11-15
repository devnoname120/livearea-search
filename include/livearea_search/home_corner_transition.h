#pragma once

#include <cstdint>

namespace livearea_search {

constexpr std::uint64_t kHomeCornerShowDurationUs = 350000;
constexpr std::uint64_t kHomeCornerHideDurationUs = 150000;

enum class HomeCornerTransitionState {
    Hidden,
    Showing,
    Visible,
    Hiding,
};

enum class HomeCornerTransitionAction {
    None,
    ShowNative,
    HideNative,
    HideImmediately,
};

enum class HomeCornerTransitionCompletion {
    None,
    Show,
    Hide,
};

HomeCornerTransitionAction RequestHomeCornerTransition(
    bool visible,
    bool immediate,
    std::uint64_t now_us,
    HomeCornerTransitionState& state,
    std::uint64_t& deadline_us) noexcept;

HomeCornerTransitionCompletion AdvanceHomeCornerTransition(
    std::uint64_t now_us,
    HomeCornerTransitionState& state,
    std::uint64_t& deadline_us) noexcept;

} // namespace livearea_search

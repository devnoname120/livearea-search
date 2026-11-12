#pragma once

#include <cstdint>

namespace livearea_search {

bool ShellAllowsOverlay();
void SuppressOverlayFor(std::uint64_t microseconds);
#if LIVEAREA_SEARCH_DIAGNOSTICS
void QueueInteractionLog(const char* message);
void QueueInteractionLogResult(const char* stage, int result);
#else
inline void QueueInteractionLog(const char*) noexcept {}
inline void QueueInteractionLogResult(const char*, int) noexcept {}
#endif

} // namespace livearea_search

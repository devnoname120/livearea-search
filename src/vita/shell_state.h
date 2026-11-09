#pragma once

#include <cstdint>

namespace livearea_search {

bool ShellAllowsOverlay();
void SuppressOverlayFor(std::uint64_t microseconds);
void QueueInteractionLog(const char* message);
void QueueInteractionLogResult(const char* stage, int result);

} // namespace livearea_search

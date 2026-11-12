#pragma once

namespace livearea_search {

#if LIVEAREA_SEARCH_DIAGNOSTICS
void ResetStartupLog();
void StartupLog(const char* message);
void StartupLogResult(const char* stage, int result);
#else
inline void ResetStartupLog() noexcept {}
inline void StartupLog(const char*) noexcept {}
inline void StartupLogResult(const char*, int) noexcept {}
#endif

} // namespace livearea_search

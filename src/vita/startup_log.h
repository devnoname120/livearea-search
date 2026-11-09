#pragma once

namespace livearea_search {

void ResetStartupLog();
void StartupLog(const char* message);
void StartupLogResult(const char* stage, int result);

} // namespace livearea_search

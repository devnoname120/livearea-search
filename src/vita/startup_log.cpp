#include "startup_log.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>

namespace livearea_search {

namespace {

const char kLogDirectory[] = "ur0:data/livearea_search";
const char kLogPath[] = "ur0:data/livearea_search/startup.log";

void AppendLine(const char* message) {
    if (message == NULL) {
        return;
    }

    sceIoMkdir(kLogDirectory, 0777);
    const SceUID fd = sceIoOpen(
        kLogPath,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
        0666);
    if (fd < 0) {
        return;
    }

    const SceSize length = static_cast<SceSize>(sceClibStrnlen(message, 240));
    sceIoWrite(fd, message, length);
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

} // namespace

void ResetStartupLog() {
    sceIoMkdir(kLogDirectory, 0777);
    const SceUID fd = sceIoOpen(
        kLogPath,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0666);
    if (fd >= 0) {
        sceIoClose(fd);
    }
}

void StartupLog(const char* message) {
    AppendLine(message);
}

void StartupLogResult(const char* stage, int result) {
    char line[256];
    sceClibSnprintf(
        line,
        sizeof(line),
        "%s: 0x%08X (%d)",
        stage == NULL ? "<null>" : stage,
        static_cast<unsigned int>(result),
        result);
    AppendLine(line);
}

} // namespace livearea_search

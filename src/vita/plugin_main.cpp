#include "paf_compat.h"
#include <psp2/appmgr.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/threadmgr.h>
#include <taihen.h>

#include <livearea_search/deferred_log_queue.h>
#include <livearea_search/lsdb_abi.h>
#include <livearea_search/module_readiness.h>

#include "paf_memory.h"
#include "config_file_failsafe.h"
#include "search_overlay.h"
#include "shell_state.h"
#include "startup_log.h"

namespace {

const char kPluginName[] = "livearea_search_plugin";
const char kResourcePath[] = "ur0:data/livearea-search/livearea_search_plugin.rco";
const std::uint32_t kShellReadyFrameThreshold = 60;

SceKernelLwMutexWork g_state_mutex;
bool g_running = false;
bool g_shell_state_valid = false;
bool g_shell_allows_overlay = false;
std::uint64_t g_suppress_until = 0;
bool g_ui_callback_registered = false;
bool g_ui_load_in_progress = false;
bool g_ui_active = false;
bool g_waiting_for_shell_logged = false;
std::uint32_t g_shell_ready_frames = 0;
livearea_search::DeferredLogQueue g_interaction_logs;

struct AppStatus {
    std::int32_t parent_process_id;
    std::uint32_t launch_flags;
    std::uint32_t protection_mode;
    char app_name[32];
    std::uint32_t has_launch_param;
    std::int32_t app_id;
    std::int32_t process_id;
    SceBool is_active;
    SceBool is_running;
    std::uint8_t reserved[64];
};

static_assert(sizeof(AppStatus) == 0x80, "SceAppMgr status ABI mismatch");

const int kAppMgrErrorNotFound = static_cast<int>(0x80802012U);

extern "C" int _sceAppMgrGetStatusByAppId(int app_id, AppStatus* app_status);

SceUID g_worker = -1;
paf::Plugin* g_plugin = NULL;
livearea_search::SearchOverlay* g_overlay = NULL;

void LockState() {
    sceKernelLockLwMutex(&g_state_mutex, 1, NULL);
}

void UnlockState() {
    sceKernelUnlockLwMutex(&g_state_mutex, 1);
}

bool IsRunning() {
    LockState();
    const bool running = g_running;
    UnlockState();
    return running;
}

void SetShellState(bool valid, bool allows_overlay) {
    LockState();
    g_shell_state_valid = valid;
    g_shell_allows_overlay = allows_overlay;
    UnlockState();
}

bool PathExists(const char* path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0;
}

void FlushInteractionLogs() {
    char line[livearea_search::DeferredLogQueue::kLineCapacity];
    for (;;) {
        LockState();
        const bool available = g_interaction_logs.Pop(line, sizeof(line));
        UnlockState();
        if (!available) {
            return;
        }
        livearea_search::StartupLog(line);
    }
}

int PollShellVisibility() {
    SceAppMgrAppState state;
    AppStatus foreground;
    sceClibMemset(&state, 0, sizeof(state));
    const int state_result = _sceAppMgrGetAppState(&state, sizeof(state), 0x03600011);
    if (state_result < 0) {
        SetShellState(false, false);
        return state_result;
    }

    sceClibMemset(&foreground, 0, sizeof(foreground));
    const int foreground_result = _sceAppMgrGetStatusByAppId(
        static_cast<int>(0xFFFFFFFDU),
        &foreground);
    if (foreground_result < 0 && foreground_result != kAppMgrErrorNotFound) {
        SetShellState(false, false);
        return foreground_result;
    }

    bool visible = foreground_result == kAppMgrErrorNotFound || !foreground.is_active;
    LockState();
    if (static_cast<std::uint64_t>(sceKernelGetSystemTimeWide()) < g_suppress_until) {
        visible = false;
    }
    g_shell_allows_overlay = visible;
    g_shell_state_valid = true;
    UnlockState();
    return 0;
}

void OnPluginStarted(paf::Plugin* plugin) {
    livearea_search::StartupLog("paf start callback entered");
    g_plugin = plugin;
    g_overlay = livearea_search::PafNew<livearea_search::SearchOverlay>(plugin);
    if (g_overlay == NULL) {
        livearea_search::StartupLog("overlay allocation failed");
        sceClibPrintf("[LiveAreaSearch] overlay allocation failed\n");
        g_plugin = NULL;
        paf::Plugin::UnloadAsync(plugin);
        return;
    }

    const int result = g_overlay->Initialize();
    livearea_search::StartupLogResult("overlay initialize", result);
    if (result < 0) {
        sceClibPrintf("[LiveAreaSearch] overlay init failed: 0x%08X\n", result);
        delete g_overlay;
        g_overlay = NULL;
        g_plugin = NULL;
        paf::Plugin::UnloadAsync(plugin);
        return;
    }

    LockState();
    g_ui_active = true;
    UnlockState();
    livearea_search::StartupLog("overlay active");
}

void UiFrame(void*) {
    paf::Plugin* top_menu = paf::Plugin::Find("topmenu_plugin");
    const bool shell_ready =
        livearea_search::ShellAllowsOverlay() &&
        livearea_search::IsPafPluginVisible(top_menu);
    bool log_waiting = false;
    LockState();
    g_shell_ready_frames = livearea_search::AdvanceShellReadyFrames(
        g_shell_ready_frames,
        shell_ready);
    const bool shell_stable = livearea_search::HasStableShellReadiness(
        g_shell_ready_frames,
        kShellReadyFrameThreshold);
    const bool should_load = livearea_search::CanLoadOverlay(
        g_running,
        g_ui_active,
        g_ui_load_in_progress,
        shell_stable);
    if (should_load) {
        g_ui_load_in_progress = true;
    } else if (
        g_running &&
        !g_ui_active &&
        !g_ui_load_in_progress &&
        !shell_stable &&
        !g_waiting_for_shell_logged) {
        g_waiting_for_shell_logged = true;
        log_waiting = true;
    }
    UnlockState();
    if (log_waiting) {
        livearea_search::StartupLog("ui frame waiting for stable LiveArea");
    }
    if (!should_load) {
        return;
    }
    livearea_search::StartupLog("ui frame LiveArea stable");

    paf::Plugin::InitParam init_param;
    init_param.name = kPluginName;
    init_param.caller_name = "__main__";
    init_param.resource_file = kResourcePath;
    init_param.start_func = OnPluginStarted;
    init_param.option = paf::Plugin::Option_None;
    init_param.draw_priority = 36;
    livearea_search::StartupLog("paf load sync begin");
    paf::Plugin::LoadSync(init_param);
    livearea_search::StartupLog("paf load sync returned");
    if (g_plugin == NULL) {
        livearea_search::StartupLog("paf resource plugin did not start");
        sceClibPrintf("[LiveAreaSearch] PAF resource plugin did not start\n");
    }

    paf::common::MainThreadCallList::Unregister(UiFrame, NULL);
    LockState();
    g_ui_callback_registered = false;
    g_ui_load_in_progress = false;
    UnlockState();
}

int WorkerMain(SceSize, void*) {
    livearea_search::StartupLog("worker entered");
    tai_module_info_t tai_paf;
    sceClibMemset(&tai_paf, 0, sizeof(tai_paf));
    tai_paf.size = sizeof(tai_paf);
    int tai_paf_result = taiGetModuleInfo("ScePaf", &tai_paf);
    livearea_search::StartupLogResult("worker initial tai ScePaf", tai_paf_result);
    while (IsRunning() &&
           !livearea_search::IsModuleLookupReady(tai_paf_result, tai_paf.modid)) {
        sceKernelDelayThread(100000);
        sceClibMemset(&tai_paf, 0, sizeof(tai_paf));
        tai_paf.size = sizeof(tai_paf);
        tai_paf_result = taiGetModuleInfo("ScePaf", &tai_paf);
    }
    if (!IsRunning()) {
        return 0;
    }
    livearea_search::StartupLogResult("worker tai ScePaf ready", tai_paf_result);
    livearea_search::StartupLogResult("worker tai ScePaf modid", tai_paf.modid);
    livearea_search::StartupLogResult(
        "worker tai ScePaf nid",
        static_cast<int>(tai_paf.module_nid));

    tai_module_info_t tai_lsdb;
    sceClibMemset(&tai_lsdb, 0, sizeof(tai_lsdb));
    tai_lsdb.size = sizeof(tai_lsdb);
    int tai_lsdb_result = taiGetModuleInfo(livearea_search::lsdb::kModuleName, &tai_lsdb);
    livearea_search::StartupLogResult("worker initial tai SceLsdb", tai_lsdb_result);
    while (IsRunning() &&
           !livearea_search::IsModuleLookupReady(tai_lsdb_result, tai_lsdb.modid)) {
        sceKernelDelayThread(100000);
        sceClibMemset(&tai_lsdb, 0, sizeof(tai_lsdb));
        tai_lsdb.size = sizeof(tai_lsdb);
        tai_lsdb_result = taiGetModuleInfo(livearea_search::lsdb::kModuleName, &tai_lsdb);
    }
    if (!IsRunning()) {
        return 0;
    }
    livearea_search::StartupLogResult("worker tai SceLsdb ready", tai_lsdb_result);
    livearea_search::StartupLogResult("worker tai SceLsdb modid", tai_lsdb.modid);
    livearea_search::StartupLogResult(
        "worker tai SceLsdb nid",
        static_cast<int>(tai_lsdb.module_nid));

    int failures = 0;
    int visibility_result = PollShellVisibility();
    while (IsRunning() && visibility_result < 0) {
        if (++failures >= 30) {
            livearea_search::StartupLogResult(
                "worker AppMgr visibility unavailable",
                visibility_result);
            sceClibPrintf("[LiveAreaSearch] AppMgr state unavailable\n");
            return 0;
        }
        sceKernelDelayThread(100000);
        visibility_result = PollShellVisibility();
    }
    livearea_search::StartupLogResult("worker AppMgr visibility", visibility_result);

    LockState();
    const bool should_register = g_running;
    if (should_register) {
        g_ui_callback_registered = true;
        g_ui_load_in_progress = true;
    }
    UnlockState();
    if (should_register) {
        livearea_search::StartupLog("main-thread callback register begin");
        paf::common::MainThreadCallList::Register(UiFrame, NULL);
        livearea_search::StartupLog("main-thread callback register returned");
        LockState();
        g_ui_load_in_progress = false;
        UnlockState();
    }
    while (IsRunning()) {
        PollShellVisibility();
        FlushInteractionLogs();
        sceKernelDelayThread(100000);
    }
    FlushInteractionLogs();
    return 0;
}

} // namespace

namespace livearea_search {

bool ShellAllowsOverlay() {
    LockState();
    const bool allowed = g_shell_state_valid && g_shell_allows_overlay;
    UnlockState();
    return allowed;
}

void SuppressOverlayFor(std::uint64_t microseconds) {
    LockState();
    g_suppress_until =
        static_cast<std::uint64_t>(sceKernelGetSystemTimeWide()) + microseconds;
    g_shell_allows_overlay = false;
    UnlockState();
}

void QueueInteractionLog(const char* message) {
    LockState();
    g_interaction_logs.Push(message);
    UnlockState();
}

void QueueInteractionLogResult(const char* stage, int result) {
    char line[DeferredLogQueue::kLineCapacity];
    sceClibSnprintf(
        line,
        sizeof(line),
        "%s: 0x%08X (%d)",
        stage == NULL ? "<null>" : stage,
        static_cast<unsigned int>(result),
        result);
    QueueInteractionLog(line);
}

} // namespace livearea_search

extern "C" int _start(SceSize, const void*)
    __attribute__((weak, alias("module_start")));

extern "C" int module_start(SceSize, const void*) {
    const int disabled_entries =
        livearea_search::DisablePluginConfigEntriesForNextBoot();
    if (disabled_entries < 0) {
        sceClibPrintf(
            "[LiveAreaSearch] config self-disable failed: 0x%08X\n",
            disabled_entries);
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    livearea_search::ResetStartupLog();
    livearea_search::StartupLogResult("module_start config entries disabled", disabled_entries);
    sceClibPrintf(
        "[LiveAreaSearch] disabled %d config entr%s for next boot\n",
        disabled_entries,
        disabled_entries == 1 ? "y" : "ies");

    if (!PathExists(kResourcePath)) {
        livearea_search::StartupLog("module_start resource missing");
        sceClibPrintf("[LiveAreaSearch] missing %s\n", kResourcePath);
        return SCE_KERNEL_START_NO_RESIDENT;
    }

    sceClibMemset(&g_state_mutex, 0, sizeof(g_state_mutex));
    const int mutex_result = sceKernelCreateLwMutex(
            &g_state_mutex,
            "LiveAreaSearchState",
            0,
            0,
            NULL);
    livearea_search::StartupLogResult("module_start state mutex", mutex_result);
    if (mutex_result < 0) {
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    LockState();
    g_running = true;
    g_shell_state_valid = false;
    g_shell_allows_overlay = false;
    g_suppress_until = 0;
    g_ui_callback_registered = false;
    g_ui_load_in_progress = false;
    g_ui_active = false;
    g_waiting_for_shell_logged = false;
    g_shell_ready_frames = 0;
    g_interaction_logs.Clear();
    UnlockState();
    g_plugin = NULL;
    g_overlay = NULL;
    g_worker = sceKernelCreateThread(
        "LiveAreaSearch",
        WorkerMain,
        0x10000100,
        0x10000,
        0,
        0,
        NULL);
    livearea_search::StartupLogResult("module_start worker create", g_worker);
    if (g_worker < 0) {
        sceKernelDeleteLwMutex(&g_state_mutex);
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    const int start_result = sceKernelStartThread(g_worker, 0, NULL);
    livearea_search::StartupLogResult("module_start worker start", start_result);
    if (start_result < 0) {
        sceKernelDeleteThread(g_worker);
        g_worker = -1;
        sceKernelDeleteLwMutex(&g_state_mutex);
        return SCE_KERNEL_START_NO_RESIDENT;
    }
    return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize, const void*) {
    LockState();
    if (g_ui_active || g_ui_load_in_progress) {
        UnlockState();
        return SCE_KERNEL_STOP_CANCEL;
    }
    g_running = false;
    UnlockState();

    if (g_worker >= 0) {
        sceKernelWaitThreadEnd(g_worker, NULL, NULL);
        sceKernelDeleteThread(g_worker);
        g_worker = -1;
    }

    LockState();
    const bool callback_registered = g_ui_callback_registered;
    g_ui_callback_registered = false;
    UnlockState();
    if (callback_registered) {
        paf::common::MainThreadCallList::Unregister(UiFrame, NULL);
    }

    sceKernelDeleteLwMutex(&g_state_mutex);
    return SCE_KERNEL_STOP_SUCCESS;
}

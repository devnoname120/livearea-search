#include "recent_apps_file.h"

#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>

namespace livearea_search {
namespace {

const char kRecentDirectory[] = "ur0:data/livearea-search";
const char kRecentPath[] = "ur0:data/livearea-search/recent_apps.txt";
const char kTemporaryPath[] = "ur0:data/livearea-search/recent_apps.txt.tmp";
const char kBackupPath[] = "ur0:data/livearea-search/recent_apps.txt.bak";
const int kErrorNoEntry = static_cast<int>(0x80010002U);
const int kErrorIo = static_cast<int>(0x80010005U);

int ReadExact(SceUID fd, char* data, SceSize size) {
    SceSize total = 0;
    while (total < size) {
        const int result = sceIoRead(fd, data + total, size - total);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            return kErrorIo;
        }
        total += static_cast<SceSize>(result);
    }
    return 0;
}

int WriteExact(SceUID fd, const char* data, SceSize size) {
    SceSize total = 0;
    while (total < size) {
        const int result = sceIoWrite(fd, data + total, size - total);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            return kErrorIo;
        }
        total += static_cast<SceSize>(result);
    }
    return 0;
}

int VerifyFile(const char* path, const char* expected, SceSize expected_size) {
    const SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }

    char bytes[RecentApps::kMaximumSerializedSize];
    int result = 0;
    const SceOff size = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (size != static_cast<SceOff>(expected_size) ||
        sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
        result = kErrorIo;
    } else {
        result = ReadExact(fd, bytes, expected_size);
    }
    if (result >= 0 && sceClibMemcmp(bytes, expected, expected_size) != 0) {
        result = kErrorIo;
    }

    const int close_result = sceIoClose(fd);
    return result < 0 ? result : close_result;
}

int FileExists(const char* path, bool& exists) {
    const SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd == kErrorNoEntry) {
        exists = false;
        return 0;
    }
    if (fd < 0) {
        return fd;
    }
    exists = true;
    return sceIoClose(fd);
}

int RemoveIfPresent(const char* path) {
    const int result = sceIoRemove(path);
    return result == kErrorNoEntry ? 0 : result;
}

int RestoreBackup() {
    bool backup_exists = false;
    int result = FileExists(kBackupPath, backup_exists);
    if (result < 0 || !backup_exists) {
        return result < 0 ? result : kErrorNoEntry;
    }

    bool recent_exists = false;
    result = FileExists(kRecentPath, recent_exists);
    if (result < 0) {
        return result;
    }

    bool displaced_recent = false;
    if (recent_exists) {
        result = RemoveIfPresent(kTemporaryPath);
        if (result < 0) {
            return result;
        }
        result = sceIoRename(kRecentPath, kTemporaryPath);
        if (result < 0) {
            return result;
        }
        displaced_recent = true;
    }

    result = sceIoRename(kBackupPath, kRecentPath);
    if (result < 0) {
        if (displaced_recent) {
            sceIoRename(kTemporaryPath, kRecentPath);
        }
        return result;
    }
    RemoveIfPresent(kTemporaryPath);
    return 0;
}

int RecoverBackupIfNeeded() {
    bool recent_exists = false;
    int result = FileExists(kRecentPath, recent_exists);
    if (result < 0 || recent_exists) {
        return result;
    }

    bool backup_exists = false;
    result = FileExists(kBackupPath, backup_exists);
    if (result < 0 || !backup_exists) {
        return result;
    }
    return RestoreBackup();
}

int ReplaceVerifiedTemporary(const char* expected, SceSize expected_size) {
    bool destination_exists = false;
    int result = FileExists(kRecentPath, destination_exists);
    if (result < 0) {
        return result;
    }

    if (!destination_exists) {
        result = sceIoRename(kTemporaryPath, kRecentPath);
        if (result >= 0) {
            result = VerifyFile(kRecentPath, expected, expected_size);
        }
        if (result < 0) {
            RemoveIfPresent(kRecentPath);
        }
        return result;
    }

    result = RemoveIfPresent(kBackupPath);
    if (result < 0) {
        return result;
    }
    result = sceIoRename(kRecentPath, kBackupPath);
    if (result < 0) {
        return result;
    }

    result = sceIoRename(kTemporaryPath, kRecentPath);
    if (result >= 0) {
        result = VerifyFile(kRecentPath, expected, expected_size);
    }
    if (result >= 0) {
        return RemoveIfPresent(kBackupPath);
    }

    const int save_error = result;
    const int restore_result = RestoreBackup();
    return restore_result < 0 ? restore_result : save_error;
}

} // namespace

int RecentAppsFile::Load(RecentApps& recent_apps) const {
    recent_apps.Clear();
    const int recovery_result = RecoverBackupIfNeeded();
    if (recovery_result < 0) {
        return recovery_result;
    }
    const SceUID fd = sceIoOpen(kRecentPath, SCE_O_RDONLY, 0);
    if (fd == kErrorNoEntry) {
        return 0;
    }
    if (fd < 0) {
        return fd;
    }

    char bytes[RecentApps::kMaximumSerializedSize];
    int result = 0;
    const SceOff size = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (size < 0 || size > static_cast<SceOff>(sizeof(bytes)) ||
        sceIoLseek(fd, 0, SCE_SEEK_SET) < 0) {
        result = kErrorIo;
    } else {
        result = ReadExact(fd, bytes, static_cast<SceSize>(size));
        if (result >= 0 && !recent_apps.Parse(bytes, static_cast<std::size_t>(size))) {
            recent_apps.Clear();
            result = 0;
        }
    }

    const int close_result = sceIoClose(fd);
    return result < 0 ? result : close_result;
}

int RecentAppsFile::Save(const RecentApps& recent_apps) const {
    const String serialized = recent_apps.Serialize();
    if (serialized.size() > RecentApps::kMaximumSerializedSize) {
        return kErrorIo;
    }

    sceIoMkdir(kRecentDirectory, 0777);
    int result = RecoverBackupIfNeeded();
    if (result < 0) {
        return result;
    }
    const SceUID fd = sceIoOpen(
        kTemporaryPath,
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
        0666);
    if (fd < 0) {
        return fd;
    }

    result = WriteExact(
        fd,
        serialized.data(),
        static_cast<SceSize>(serialized.size()));
    if (result >= 0) {
        result = sceIoSyncByFd(fd, 0);
    }
    const int close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) {
        result = close_result;
    }
    if (result >= 0) {
        result = VerifyFile(
            kTemporaryPath,
            serialized.data(),
            static_cast<SceSize>(serialized.size()));
    }
    if (result >= 0) {
        result = ReplaceVerifiedTemporary(
            serialized.data(),
            static_cast<SceSize>(serialized.size()));
    }
    if (result < 0) {
        sceIoRemove(kTemporaryPath);
    }
    return result;
}

} // namespace livearea_search

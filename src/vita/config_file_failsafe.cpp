#include "config_file_failsafe.h"

#include <livearea_search/config_failsafe.h>

#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>

#include <cstddef>
#include <cstdint>

namespace livearea_search {
namespace {

const char kConfigPath[] = "ur0:tai/config.txt";
const SceOff kMaximumConfigSize = 1024 * 1024;
const int kErrorNoEntry = static_cast<int>(0x80010002U);
const int kErrorIo = static_cast<int>(0x80010005U);
const int kErrorInvalidArgument = static_cast<int>(0x80010016U);
const int kErrorFileTooLarge = static_cast<int>(0x8001001BU);

SceSize AlignToPage(SceSize size) {
    return (size + 0xFFFU) & ~0xFFFU;
}

int VerifyConfigFile(
    SceUID fd,
    const std::uint8_t* expected,
    SceSize expected_size) {
    const SceOff actual_size = sceIoLseek(fd, 0, SCE_SEEK_END);
    if (actual_size < 0) {
        return static_cast<int>(actual_size);
    }
    if (actual_size != static_cast<SceOff>(expected_size)) {
        return kErrorIo;
    }

    std::uint8_t buffer[256];
    SceSize offset = 0;
    while (offset < expected_size) {
        const SceSize remaining = expected_size - offset;
        const SceSize chunk_size =
            remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        const int read_result = sceIoPread(
            fd,
            buffer,
            chunk_size,
            static_cast<SceOff>(offset));
        if (read_result < 0) {
            return read_result;
        }
        if (read_result != static_cast<int>(chunk_size)) {
            return kErrorIo;
        }
        for (SceSize index = 0; index < chunk_size; ++index) {
            if (buffer[index] != expected[offset + index]) {
                return kErrorIo;
            }
        }
        offset += chunk_size;
    }
    return 0;
}

} // namespace

int DisablePluginConfigEntriesForNextBoot() {
    const SceUID fd = sceIoOpen(kConfigPath, SCE_O_RDWR, 0);
    if (fd < 0) {
        return fd;
    }

    int result = 0;
    SceUID memory_block = -1;
    void* memory = nullptr;

    do {
        const SceOff file_size = sceIoLseek(fd, 0, SCE_SEEK_END);
        if (file_size < 0) {
            result = static_cast<int>(file_size);
            break;
        }
        if (file_size == 0) {
            result = kErrorNoEntry;
            break;
        }
        if (file_size > kMaximumConfigSize) {
            result = kErrorFileTooLarge;
            break;
        }

        const SceSize config_size = static_cast<SceSize>(file_size);
        memory_block = sceKernelAllocMemBlock(
            "LiveAreaSearchConfig",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
            AlignToPage(config_size),
            nullptr);
        if (memory_block < 0) {
            result = memory_block;
            break;
        }

        result = sceKernelGetMemBlockBase(memory_block, &memory);
        if (result < 0 || memory == nullptr) {
            if (result >= 0) {
                result = kErrorIo;
            }
            break;
        }

        std::uint8_t* const bytes = static_cast<std::uint8_t*>(memory);
        SceSize total_read = 0;
        while (total_read < config_size) {
            const int read_result = sceIoPread(
                fd,
                bytes + total_read,
                config_size - total_read,
                static_cast<SceOff>(total_read));
            if (read_result < 0) {
                result = read_result;
                break;
            }
            if (read_result == 0) {
                result = kErrorIo;
                break;
            }
            total_read += static_cast<SceSize>(read_result);
        }
        if (result < 0) {
            break;
        }
        for (SceSize index = 0; index < config_size; ++index) {
            if (bytes[index] == 0) {
                result = kErrorInvalidArgument;
                break;
            }
        }
        if (result < 0) {
            break;
        }

        const std::size_t entry_count = CountConfigSelfDisableEntries(
            reinterpret_cast<const char*>(bytes),
            static_cast<std::size_t>(config_size));
        if (entry_count == 0) {
            result = kErrorNoEntry;
            break;
        }
        if (entry_count != 1) {
            result = kErrorInvalidArgument;
            break;
        }

        result = VerifyConfigFile(fd, bytes, config_size);
        if (result < 0) {
            break;
        }

        const std::size_t offset = FindConfigSelfDisableOffset(
            reinterpret_cast<const char*>(bytes),
            static_cast<std::size_t>(config_size));
        const char marker = '#';
        const int write_result = sceIoPwrite(
            fd,
            &marker,
            1,
            static_cast<SceOff>(offset));
        if (write_result != 1) {
            result = write_result < 0 ? write_result : kErrorIo;
            break;
        }
        bytes[offset] = static_cast<std::uint8_t>(marker);

        result = sceIoSyncByFd(fd, 0);
        if (result < 0) {
            break;
        }
        result = VerifyConfigFile(fd, bytes, config_size);
        if (result < 0) {
            break;
        }
        result = 1;
    } while (false);

    if (memory_block >= 0) {
        const int free_result = sceKernelFreeMemBlock(memory_block);
        if (result >= 0 && free_result < 0) {
            result = free_result;
        }
    }

    const int close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) {
        result = close_result;
    }
    return result;
}

} // namespace livearea_search

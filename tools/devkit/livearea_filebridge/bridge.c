#include "bridge_io.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FILE_BRIDGE_HOST_TEST
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>

struct bridge_directory {
    SceUID value;
};

int bridge_io_open(const char *path, unsigned flags, unsigned mode) {
    int native = flags & BRIDGE_OPEN_WRITE ? SCE_O_WRONLY : SCE_O_RDONLY;
    if (flags & BRIDGE_OPEN_CREATE) native |= SCE_O_CREAT;
    if (flags & BRIDGE_OPEN_TRUNCATE) native |= SCE_O_TRUNC;
    if (flags & BRIDGE_OPEN_EXCLUSIVE) native |= SCE_O_EXCL;
    return sceIoOpen(path, native, mode);
}

int bridge_io_read(int fd, void *data, unsigned size) {
    return sceIoRead(fd, data, size);
}

int bridge_io_write(int fd, const void *data, unsigned size) {
    return sceIoWrite(fd, data, size);
}

int bridge_io_close(int fd) {
    return sceIoClose(fd);
}

int bridge_io_sync(int fd) {
    return sceIoSyncByFd(fd, 0);
}

int bridge_io_stat(const char *path, uint64_t *size, int *directory) {
    SceIoStat value;
    memset(&value, 0, sizeof(value));
    int result = sceIoGetstat(path, &value);
    if (result < 0) return result;
    if (size) *size = value.st_size;
    if (directory) *directory = SCE_S_ISDIR(value.st_mode);
    return 0;
}

int bridge_io_remove(const char *path) {
    return sceIoRemove(path);
}

int bridge_io_mkdir(const char *path, unsigned mode) {
    return sceIoMkdir(path, mode);
}

int bridge_io_is_missing(int result) {
    return (uint32_t)result == UINT32_C(0x80010002);
}

bridge_directory *bridge_io_dopen(const char *path) {
    SceUID fd = sceIoDopen(path);
    if (fd < 0) return NULL;
    bridge_directory *result = malloc(sizeof(*result));
    if (!result) {
        sceIoDclose(fd);
        return NULL;
    }
    result->value = fd;
    return result;
}

int bridge_io_dread(bridge_directory *directory, bridge_directory_entry *entry) {
    SceIoDirent value;
    memset(&value, 0, sizeof(value));
    int result = sceIoDread(directory->value, &value);
    if (result <= 0) return result;
    size_t length = strnlen(value.d_name, sizeof(value.d_name));
    if (!length || length >= sizeof(entry->name)) return -ENAMETOOLONG;
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, value.d_name, length + 1u);
    entry->size = value.d_stat.st_size;
    entry->directory = SCE_S_ISDIR(value.d_stat.st_mode);
    return 1;
}

int bridge_io_dclose(bridge_directory *directory) {
    int result = sceIoDclose(directory->value);
    free(directory);
    return result;
}
#endif

#define REQUEST_MAGIC UINT32_C(0x31464242)
#define RESULT_MAGIC UINT32_C(0x31524242)
#define FORMAT_VERSION 1u
#define MAX_ENTRIES 72u
#define MAX_PAYLOAD UINT32_C(8388608)
#define MAX_READ UINT32_C(67108864)
#define SKIPPED_STATUS (-9999)
#define ROLLBACK_FAILED_STATUS (-12001)

#define OP_READ 1u
#define OP_REPLACE 2u
#define OP_LIST 3u
#define OP_INSTALL 4u
#define OP_ENSURE_DIR 5u
#define OP_READ_OPTIONAL 6u

typedef struct request_header {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t job;
} request_header;

typedef struct request_entry {
    uint32_t operation;
    uint32_t limit;
    uint32_t new_size;
    uint32_t old_size;
    char path[256];
} request_entry;

typedef struct result_entry {
    int32_t status;
    uint32_t size;
} result_entry;

typedef struct result_header {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t job;
} result_header;

_Static_assert(sizeof(request_entry) == 272u, "request entry layout");
_Static_assert(sizeof(result_entry) == 8u, "result entry layout");

static uint8_t payload[MAX_PAYLOAD];
static uint8_t block_a[65536];
static uint8_t block_b[65536];
static request_entry requests[MAX_ENTRIES];
static result_entry results[MAX_ENTRIES];

static int write_all(int fd, const void *data, uint32_t size) {
    const uint8_t *cursor = data;
    while (size) {
        int written = bridge_io_write(fd, cursor, size);
        if (written <= 0 || (uint32_t)written > size) return written < 0 ? written : -EIO;
        cursor += (uint32_t)written;
        size -= (uint32_t)written;
    }
    return 0;
}

static int read_exact(int fd, void *data, uint32_t size) {
    uint8_t *cursor = data;
    while (size) {
        int count = bridge_io_read(fd, cursor, size);
        if (count <= 0 || (uint32_t)count > size) return count < 0 ? count : -EIO;
        cursor += (uint32_t)count;
        size -= (uint32_t)count;
    }
    uint8_t extra;
    int trailing = bridge_io_read(fd, &extra, 1);
    return trailing == 0 ? 0 : trailing < 0 ? trailing : -EFBIG;
}

static int close_with(int fd, int result) {
    int closed = bridge_io_close(fd);
    return result < 0 ? result : closed;
}

static int regular_size(const char *path, uint64_t *size) {
    int directory = 0;
    int result = bridge_io_stat(path, size, &directory);
    if (result < 0) return result;
    return directory ? -EISDIR : 0;
}

static int safe_path(const char *path) {
    if (!path[0] || strchr(path, '\\') || strchr(path, '\r') || strchr(path, '\n')) return 0;
    const char *colon = strchr(path, ':');
    if (!colon || colon == path || colon[1] != '/' || strchr(colon + 1, ':')) return 0;
    const char *segment = colon + 2;
    while (*segment) {
        const char *slash = strchr(segment, '/');
        size_t length = slash ? (size_t)(slash - segment) : strlen(segment);
        if (!length || (length == 1u && segment[0] == '.') ||
            (length == 2u && segment[0] == '.' && segment[1] == '.')) return 0;
        for (size_t i = 0; i < length; ++i) {
            unsigned char value = (unsigned char)segment[i];
            if (value < 0x20u || value == 0x7fu) return 0;
        }
        if (!slash) break;
        segment = slash + 1;
    }
    return 1;
}

static int exact_path(const char *path, const char *expected) {
    return safe_path(path) && strcmp(path, expected) == 0;
}

static int allowed_read(const char *path) {
    static const char *const exact[] = {
        "ur0:/tai/config.txt",
        "ur0:/tai/livearea_search/livearea_search.suprx",
        "ur0:/tai/livearea_search/livearea_search_plugin.rco",
        "ur0:/data/livearea_search/startup.log",
    };
    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i)
        if (exact_path(path, exact[i])) return 1;
    return 0;
}

static int allowed_replace(const char *path) {
    return exact_path(path, "ur0:/tai/config.txt") ||
           exact_path(path, "ur0:/tai/livearea_search/livearea_search.suprx") ||
           exact_path(path, "ur0:/tai/livearea_search/livearea_search_plugin.rco");
}

static int allowed_directory(const char *path) {
    return exact_path(path, "ur0:/data/livearea_search") ||
           exact_path(path, "ur0:/tai/livearea_search");
}

static int entry_path(const request_entry *entry, char output[257]) {
    const char *end = memchr(entry->path, 0, sizeof(entry->path));
    if (!end) return -EINVAL;
    size_t length = (size_t)(end - entry->path);
    for (size_t i = length + 1u; i < sizeof(entry->path); ++i)
        if (entry->path[i]) return -EINVAL;
    memcpy(output, entry->path, length + 1u);
    return safe_path(output) ? 0 : -EINVAL;
}

static int artifact_path(char output[96], uint32_t job, uint32_t index, const char *suffix) {
    int length = snprintf(output, 96, "host0:/bridge/%08u-%03u.%s", job, index, suffix);
    return length > 0 && length < 96 ? 0 : -ENAMETOOLONG;
}

static int stream_copy(int input, int output, uint32_t expected, uint32_t *written) {
    uint32_t total = 0;
    while (total < expected) {
        uint32_t want = expected - total;
        if (want > sizeof(block_a)) want = sizeof(block_a);
        int count = bridge_io_read(input, block_a, want);
        if (count <= 0 || (uint32_t)count > want) return count < 0 ? count : -EIO;
        int result = write_all(output, block_a, (uint32_t)count);
        if (result < 0) return result;
        total += (uint32_t)count;
    }
    uint8_t extra;
    int trailing = bridge_io_read(input, &extra, 1);
    if (trailing != 0) return trailing < 0 ? trailing : -EFBIG;
    *written = total;
    return 0;
}

static int read_operation(uint32_t job, uint32_t index, const request_entry *entry, const char *path,
                          uint32_t *size) {
    if (!allowed_read(path) || !entry->limit || entry->limit > MAX_READ || entry->new_size || entry->old_size)
        return -EINVAL;
    uint64_t length = 0;
    int result = regular_size(path, &length);
    if (result < 0) return result;
    if (length > entry->limit || length > UINT32_MAX) return -EFBIG;
    char output_path[96];
    result = artifact_path(output_path, job, index, "out");
    if (result < 0) return result;
    int input = bridge_io_open(path, BRIDGE_OPEN_READ, 0);
    if (input < 0) return input;
    int output = bridge_io_open(output_path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE, 0666);
    if (output < 0) return close_with(input, output);
    result = stream_copy(input, output, (uint32_t)length, size);
    result = close_with(input, result);
    return close_with(output, result);
}

static int optional_read_operation(
    uint32_t job,
    uint32_t index,
    const request_entry *entry,
    const char *path,
    uint32_t *size) {
    if (!allowed_read(path) || !entry->limit || entry->limit > MAX_READ ||
        entry->new_size || entry->old_size)
        return -EINVAL;
    uint64_t length = 0;
    int result = regular_size(path, &length);
    if (result >= 0) return read_operation(job, index, entry, path, size);
    if (!bridge_io_is_missing(result)) return result;
    char output_path[96];
    result = artifact_path(output_path, job, index, "out");
    if (result < 0) return result;
    int output = bridge_io_open(
        output_path,
        BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE,
        0666);
    if (output < 0) return output;
    *size = 0;
    return close_with(output, 0);
}

static int compare_files(const char *left_path, const char *right_path, uint32_t expected) {
    uint64_t left_size = 0, right_size = 0;
    int result = regular_size(left_path, &left_size);
    if (result < 0) return result;
    result = regular_size(right_path, &right_size);
    if (result < 0) return result;
    if (left_size != expected || right_size != expected) return -EMSGSIZE;
    int left = bridge_io_open(left_path, BRIDGE_OPEN_READ, 0);
    if (left < 0) return left;
    int right = bridge_io_open(right_path, BRIDGE_OPEN_READ, 0);
    if (right < 0) return close_with(left, right);
    uint32_t remaining = expected;
    while (remaining && result >= 0) {
        uint32_t want = remaining > sizeof(block_a) ? sizeof(block_a) : remaining;
        int a = bridge_io_read(left, block_a, want);
        int b = bridge_io_read(right, block_b, want);
        if (a != (int)want || b != (int)want) result = a < 0 ? a : b < 0 ? b : -EIO;
        else if (memcmp(block_a, block_b, want)) result = -EBADMSG;
        remaining -= want;
    }
    result = close_with(left, result);
    return close_with(right, result);
}

static int load_payload(const char *path, uint32_t expected) {
    uint64_t length = 0;
    int result = regular_size(path, &length);
    if (result < 0) return result;
    if (length != expected || expected > MAX_PAYLOAD) return -EMSGSIZE;
    int input = bridge_io_open(path, BRIDGE_OPEN_READ, 0);
    if (input < 0) return input;
    result = read_exact(input, payload, expected);
    return close_with(input, result);
}

static int restore_file(const char *expected_path, const char *path, uint32_t expected) {
    int input = bridge_io_open(expected_path, BRIDGE_OPEN_READ, 0);
    if (input < 0) return input;
    int output = bridge_io_open(path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_TRUNCATE, 0666);
    if (output < 0) return close_with(input, output);
    uint32_t written = 0;
    int result = stream_copy(input, output, expected, &written);
    result = close_with(input, result);
    if (result >= 0) result = bridge_io_sync(output);
    result = close_with(output, result);
    if (result >= 0 && written != expected) result = -EMSGSIZE;
    if (result >= 0) result = compare_files(path, expected_path, expected);
    return result;
}

static int replace_operation(uint32_t job, uint32_t index, const request_entry *entry, const char *path,
                             uint32_t *size) {
    if (!allowed_replace(path) || !entry->limit || entry->limit > MAX_PAYLOAD ||
        !entry->new_size || entry->new_size > entry->limit ||
        entry->old_size > entry->limit) return -EINVAL;
    char expected_path[96], payload_path[96], output_path[96];
    int result = artifact_path(expected_path, job, index, "expected");
    if (result >= 0) result = artifact_path(payload_path, job, index, "payload");
    if (result >= 0) result = artifact_path(output_path, job, index, "out");
    if (result < 0) return result;
    result = compare_files(path, expected_path, entry->old_size);
    if (result < 0) return result;
    result = load_payload(payload_path, entry->new_size);
    if (result < 0) return result;
    int output = bridge_io_open(output_path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE, 0666);
    if (output < 0) return output;
    int remote = bridge_io_open(path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_TRUNCATE, 0666);
    if (remote < 0) return close_with(output, remote);
    result = write_all(remote, payload, entry->new_size);
    if (result >= 0) result = bridge_io_sync(remote);
    result = close_with(remote, result);
    if (result >= 0) {
        uint64_t length = 0;
        result = regular_size(path, &length);
        if (result >= 0 && length != entry->new_size) result = -EMSGSIZE;
    }
    if (result >= 0) {
        remote = bridge_io_open(path, BRIDGE_OPEN_READ, 0);
        if (remote < 0) result = remote;
        else {
            uint32_t total = 0;
            while (total < entry->new_size && result >= 0) {
                uint32_t want = entry->new_size - total;
                if (want > sizeof(block_a)) want = sizeof(block_a);
                int count = bridge_io_read(remote, block_a, want);
                if (count != (int)want) result = count < 0 ? count : -EIO;
                else if (memcmp(block_a, payload + total, want)) result = -EBADMSG;
                else result = write_all(output, block_a, want);
                total += want;
            }
            if (result >= 0) {
                uint8_t extra;
                int trailing = bridge_io_read(remote, &extra, 1);
                if (trailing != 0) result = trailing < 0 ? trailing : -EFBIG;
            }
            result = close_with(remote, result);
        }
    }
    result = close_with(output, result);
    if (result < 0) {
        int restored = restore_file(expected_path, path, entry->old_size);
        if (restored < 0) return ROLLBACK_FAILED_STATUS;
    } else {
        *size = entry->new_size;
    }
    return result;
}

static int install_operation(uint32_t job, uint32_t index, const request_entry *entry, const char *path,
                             uint32_t *size) {
    if (!allowed_replace(path) || !entry->limit || entry->limit > MAX_PAYLOAD ||
        !entry->new_size || entry->new_size > entry->limit || entry->old_size) return -EINVAL;
    uint64_t existing_size = 0;
    int result = regular_size(path, &existing_size);
    if (result >= 0) return -EEXIST;
    if (!bridge_io_is_missing(result)) return result;
    char payload_path[96], output_path[96];
    result = artifact_path(payload_path, job, index, "payload");
    if (result >= 0) result = artifact_path(output_path, job, index, "out");
    if (result < 0) return result;
    result = load_payload(payload_path, entry->new_size);
    if (result < 0) return result;
    int evidence = bridge_io_open(
        output_path,
        BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE,
        0666);
    if (evidence < 0) return evidence;
    int remote = bridge_io_open(
        path,
        BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE,
        0666);
    if (remote < 0) return close_with(evidence, remote);
    result = write_all(remote, payload, entry->new_size);
    if (result >= 0) result = bridge_io_sync(remote);
    result = close_with(remote, result);
    if (result >= 0) result = compare_files(path, payload_path, entry->new_size);
    if (result >= 0) result = write_all(evidence, payload, entry->new_size);
    result = close_with(evidence, result);
    if (result < 0) {
        int removed = bridge_io_remove(path);
        if (removed < 0 && !bridge_io_is_missing(removed)) return ROLLBACK_FAILED_STATUS;
    } else {
        *size = entry->new_size;
    }
    return result;
}

static int ensure_directory_operation(const request_entry *entry, const char *path) {
    if (!allowed_directory(path) || entry->limit || entry->new_size || entry->old_size)
        return -EINVAL;
    uint64_t size = 0;
    int directory = 0;
    int result = bridge_io_stat(path, &size, &directory);
    if (result >= 0) return directory ? 0 : -ENOTDIR;
    if (!bridge_io_is_missing(result)) return result;
    return bridge_io_mkdir(path, 0777);
}

static int valid_name(const char *name) {
    if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        if (*p < 0x20u || *p == 0x7fu || *p == '/' || *p == '\\') return 0;
    return 1;
}

static int list_write(int output, const char *kind, uint64_t size, const char *relative,
                      uint32_t limit, uint32_t *written) {
    char line[896];
    int length = snprintf(line, sizeof(line), "%s\t%llu\t%s\n", kind,
                          (unsigned long long)size, relative);
    if (length < 0 || (size_t)length >= sizeof(line)) return -ENAMETOOLONG;
    if ((uint32_t)length > limit - *written) return -EFBIG;
    int result = write_all(output, line, (uint32_t)length);
    if (result >= 0) *written += (uint32_t)length;
    return result;
}

static int list_tree(int output, const char *base, const char *relative, unsigned depth,
                     uint32_t limit, uint32_t *written, uint32_t *population) {
    if (depth > 8u || *population >= 512u) return -E2BIG;
    char directory_path[768];
    int length = relative[0] ? snprintf(directory_path, sizeof(directory_path), "%s/%s", base, relative)
                             : snprintf(directory_path, sizeof(directory_path), "%s", base);
    if (length < 0 || (size_t)length >= sizeof(directory_path)) return -ENAMETOOLONG;
    bridge_directory *directory = bridge_io_dopen(directory_path);
    if (!directory) return -EIO;
    int result = 0;
    bridge_directory_entry entry;
    while ((result = bridge_io_dread(directory, &entry)) > 0) {
        if (!valid_name(entry.name)) {
            if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) continue;
            result = -EINVAL;
            break;
        }
        if (++*population > 512u) {
            result = -E2BIG;
            break;
        }
        char child[640];
        length = relative[0] ? snprintf(child, sizeof(child), "%s/%s", relative, entry.name)
                             : snprintf(child, sizeof(child), "%s", entry.name);
        if (length < 0 || (size_t)length >= sizeof(child)) {
            result = -ENAMETOOLONG;
            break;
        }
        result = list_write(output, entry.directory ? "D" : "F", entry.directory ? 0 : entry.size,
                            child, limit, written);
        if (result < 0) break;
        if (entry.directory) {
            result = list_tree(output, base, child, depth + 1u, limit, written, population);
            if (result < 0) break;
        }
    }
    int closed = bridge_io_dclose(directory);
    if (result == 0 && closed < 0) result = closed;
    return result < 0 ? result : 0;
}

static int list_operation(uint32_t job, uint32_t index, const request_entry *entry, const char *path,
                          uint32_t *size) {
    if ((!exact_path(path, "ux0:/data") &&
         !exact_path(path, "ur0:/data/livearea_search")) || !entry->limit ||
        entry->limit > MAX_READ || entry->new_size || entry->old_size) return -EINVAL;
    char output_path[96];
    int result = artifact_path(output_path, job, index, "out");
    if (result < 0) return result;
    int output = bridge_io_open(output_path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE, 0666);
    if (output < 0) return output;
    uint32_t written = 0, population = 0;
    result = list_tree(output, path, "", 0, entry->limit, &written, &population);
    result = close_with(output, result);
    if (result >= 0) *size = written;
    return result;
}

static int perform(uint32_t job, uint32_t index, const request_entry *entry, uint32_t *size) {
    char path[257];
    int result = entry_path(entry, path);
    if (result < 0) return result;
    if (entry->operation == OP_READ) return read_operation(job, index, entry, path, size);
    if (entry->operation == OP_REPLACE) return replace_operation(job, index, entry, path, size);
    if (entry->operation == OP_LIST) return list_operation(job, index, entry, path, size);
    if (entry->operation == OP_INSTALL) return install_operation(job, index, entry, path, size);
    if (entry->operation == OP_ENSURE_DIR) return ensure_directory_operation(entry, path);
    if (entry->operation == OP_READ_OPTIONAL)
        return optional_read_operation(job, index, entry, path, size);
    return -EINVAL;
}

static int read_request(request_header *header) {
    uint64_t length = 0;
    int result = regular_size("host0:/bridge/request.bin", &length);
    if (result < 0) return result;
    if (length < sizeof(*header) || length > sizeof(*header) + sizeof(requests)) return -EMSGSIZE;
    int input = bridge_io_open("host0:/bridge/request.bin", BRIDGE_OPEN_READ, 0);
    if (input < 0) return input;
    uint32_t header_bytes = 0;
    while (header_bytes < sizeof(*header)) {
        uint32_t remaining = (uint32_t)sizeof(*header) - header_bytes;
        int count = bridge_io_read(input, (uint8_t *)header + header_bytes, remaining);
        if (count <= 0 || (uint32_t)count > remaining) {
            result = count < 0 ? count : -EIO;
            break;
        }
        header_bytes += (uint32_t)count;
    }
    uint32_t bytes = (uint32_t)length - (uint32_t)sizeof(*header);
    if (result >= 0) result = read_exact(input, requests, bytes);
    return close_with(input, result);
}

static int write_result(const request_header *request) {
    char path[96];
    int length = snprintf(path, sizeof(path), "host0:/bridge/%08u.done", request->job);
    if (length <= 0 || length >= (int)sizeof(path)) return -ENAMETOOLONG;
    int output = bridge_io_open(path, BRIDGE_OPEN_WRITE | BRIDGE_OPEN_CREATE | BRIDGE_OPEN_EXCLUSIVE, 0666);
    if (output < 0) return output;
    result_header header = {RESULT_MAGIC, FORMAT_VERSION, request->count, request->job};
    int result = write_all(output, &header, sizeof(header));
    if (result >= 0) result = write_all(output, results, request->count * sizeof(results[0]));
    return close_with(output, result);
}

int main(void) {
    request_header request;
    memset(&request, 0, sizeof(request));
    memset(requests, 0, sizeof(requests));
    memset(results, 0, sizeof(results));
    int result = read_request(&request);
    if (result < 0 || request.magic != REQUEST_MAGIC || request.version != FORMAT_VERSION ||
        !request.job || !request.count || request.count > MAX_ENTRIES) return 1;
    uint64_t expected = sizeof(request) + (uint64_t)request.count * sizeof(requests[0]);
    uint64_t actual = 0;
    if (regular_size("host0:/bridge/request.bin", &actual) < 0 || actual != expected) return 1;
    int failed = 0;
    for (uint32_t i = 0; i < request.count; ++i) {
        results[i].status = SKIPPED_STATUS;
        if (failed) continue;
        results[i].size = 0;
        results[i].status = perform(request.job, i, &requests[i], &results[i].size);
        failed = results[i].status < 0;
    }
    result = write_result(&request);
#ifndef FILE_BRIDGE_HOST_TEST
    sceKernelExitProcess(result < 0 ? 1 : 0);
#endif
    return result < 0 ? 1 : 0;
}

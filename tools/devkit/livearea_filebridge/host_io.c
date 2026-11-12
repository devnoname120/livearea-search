#include "bridge_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct bridge_directory {
    DIR *value;
    char path[PATH_MAX];
};

static unsigned write_call_count;

static unsigned selected_write_call(const char *name) {
    const char *value = getenv(name);
    if (!value || !value[0]) return 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    return end && !*end && parsed <= UINT_MAX ? (unsigned)parsed : 0;
}

static int local_path(const char *path, char output[PATH_MAX]) {
    const char *colon = strchr(path, ':');
    if (!colon || colon == path || colon[1] != '/' || strchr(colon + 2, ':')) return -EINVAL;
    size_t prefix = (size_t)(colon - path);
    size_t suffix = strlen(colon + 2);
    if (prefix + 1u + suffix + 1u > PATH_MAX) return -ENAMETOOLONG;
    memcpy(output, path, prefix);
    output[prefix] = '/';
    memcpy(output + prefix + 1u, colon + 2, suffix + 1u);
    return 0;
}

static int negative_errno(void) {
    return errno ? -errno : -EIO;
}

int bridge_io_open(const char *path, unsigned flags, unsigned mode) {
    char local[PATH_MAX];
    int converted = local_path(path, local);
    if (converted < 0) return converted;
    int native = 0;
    if (flags & BRIDGE_OPEN_WRITE) native |= O_WRONLY;
    else native |= O_RDONLY;
    if (flags & BRIDGE_OPEN_CREATE) native |= O_CREAT;
    if (flags & BRIDGE_OPEN_TRUNCATE) native |= O_TRUNC;
    if (flags & BRIDGE_OPEN_EXCLUSIVE) native |= O_EXCL;
#ifdef O_NOFOLLOW
    native |= O_NOFOLLOW;
#endif
    int fd = open(local, native, mode);
    return fd < 0 ? negative_errno() : fd;
}

int bridge_io_read(int fd, void *data, unsigned size) {
    ssize_t result = read(fd, data, size);
    return result < 0 ? negative_errno() : (int)result;
}

int bridge_io_write(int fd, const void *data, unsigned size) {
    unsigned call = ++write_call_count;
    if (selected_write_call("BRIDGE_FAIL_WRITE_CALL") == call) return -EIO;
    if (selected_write_call("BRIDGE_OVERSIZE_WRITE_CALL") == call)
        return size == UINT_MAX ? INT_MAX : (int)size + 1;
    const char *short_write = getenv("BRIDGE_SHORT_WRITE");
    if (short_write && short_write[0] && size > 1u) size = size / 2u;
    const char *oversized = getenv("BRIDGE_OVERSIZE_WRITE");
    if (oversized && oversized[0]) return size == UINT_MAX ? INT_MAX : (int)size + 1;
    ssize_t result = write(fd, data, size);
    return result < 0 ? negative_errno() : (int)result;
}

int bridge_io_close(int fd) {
    return close(fd) < 0 ? negative_errno() : 0;
}

int bridge_io_sync(int fd) {
    return fsync(fd) < 0 ? negative_errno() : 0;
}

int bridge_io_stat(const char *path, uint64_t *size, int *directory) {
    char local[PATH_MAX];
    int converted = local_path(path, local);
    if (converted < 0) return converted;
    struct stat value;
    if (lstat(local, &value) < 0) return negative_errno();
    if (S_ISLNK(value.st_mode)) return -ELOOP;
    if (!S_ISREG(value.st_mode) && !S_ISDIR(value.st_mode)) return -EINVAL;
    if (size) *size = (uint64_t)value.st_size;
    if (directory) *directory = S_ISDIR(value.st_mode);
    return 0;
}

int bridge_io_remove(const char *path) {
    char local[PATH_MAX];
    int converted = local_path(path, local);
    if (converted < 0) return converted;
    return unlink(local) < 0 ? negative_errno() : 0;
}

int bridge_io_mkdir(const char *path, unsigned mode) {
    char local[PATH_MAX];
    int converted = local_path(path, local);
    if (converted < 0) return converted;
    return mkdir(local, mode) < 0 ? negative_errno() : 0;
}

int bridge_io_is_missing(int result) {
    return result == -ENOENT;
}

bridge_directory *bridge_io_dopen(const char *path) {
    char local[PATH_MAX];
    if (local_path(path, local) < 0) return NULL;
    bridge_directory *result = calloc(1, sizeof(*result));
    if (!result) return NULL;
    size_t length = strlen(local);
    if (length >= sizeof(result->path)) {
        free(result);
        return NULL;
    }
    memcpy(result->path, local, length + 1u);
    result->value = opendir(local);
    if (!result->value) {
        free(result);
        return NULL;
    }
    return result;
}

int bridge_io_dread(bridge_directory *directory, bridge_directory_entry *entry) {
    errno = 0;
    struct dirent *next = readdir(directory->value);
    if (!next) return errno ? negative_errno() : 0;
    size_t name_length = strlen(next->d_name);
    if (!name_length || name_length >= sizeof(entry->name)) return -ENAMETOOLONG;
    char child[PATH_MAX];
    int length = snprintf(child, sizeof(child), "%s/%s", directory->path, next->d_name);
    if (length < 0 || (size_t)length >= sizeof(child)) return -ENAMETOOLONG;
    struct stat value;
    if (lstat(child, &value) < 0) return negative_errno();
    if (S_ISLNK(value.st_mode)) return -ELOOP;
    if (!S_ISREG(value.st_mode) && !S_ISDIR(value.st_mode)) return -EINVAL;
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->name, next->d_name, name_length + 1u);
    entry->size = (uint64_t)value.st_size;
    entry->directory = S_ISDIR(value.st_mode);
    return 1;
}

int bridge_io_dclose(bridge_directory *directory) {
    int result = closedir(directory->value) < 0 ? negative_errno() : 0;
    free(directory);
    return result;
}

#ifndef LIVEAREA_SEARCH_FILE_BRIDGE_IO_H
#define LIVEAREA_SEARCH_FILE_BRIDGE_IO_H

#include <stddef.h>
#include <stdint.h>

#define BRIDGE_OPEN_READ 1u
#define BRIDGE_OPEN_WRITE 2u
#define BRIDGE_OPEN_CREATE 4u
#define BRIDGE_OPEN_TRUNCATE 8u
#define BRIDGE_OPEN_EXCLUSIVE 16u

typedef struct bridge_directory bridge_directory;

typedef struct bridge_directory_entry {
    char name[256];
    uint64_t size;
    int directory;
} bridge_directory_entry;

int bridge_io_open(const char *path, unsigned flags, unsigned mode);
int bridge_io_read(int fd, void *data, unsigned size);
int bridge_io_write(int fd, const void *data, unsigned size);
int bridge_io_close(int fd);
int bridge_io_sync(int fd);
int bridge_io_stat(const char *path, uint64_t *size, int *directory);
int bridge_io_remove(const char *path);
int bridge_io_mkdir(const char *path, unsigned mode);
int bridge_io_is_missing(int result);
bridge_directory *bridge_io_dopen(const char *path);
int bridge_io_dread(bridge_directory *directory, bridge_directory_entry *entry);
int bridge_io_dclose(bridge_directory *directory);

#endif

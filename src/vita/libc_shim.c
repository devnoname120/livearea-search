#include <psp2/kernel/clib.h>

#include <stddef.h>

void* memcpy(void* destination, const void* source, size_t size) {
    return sceClibMemcpy(destination, source, size);
}

void* memmove(void* destination, const void* source, size_t size) {
    return sceClibMemmove(destination, source, size);
}

void* memset(void* destination, int value, size_t size) {
    return sceClibMemset(destination, value, size);
}

int memcmp(const void* left, const void* right, size_t size) {
    return sceClibMemcmp(left, right, size);
}

void* memchr(const void* memory, int value, size_t size) {
    return sceClibMemchr(memory, value, size);
}

size_t strlen(const char* text) {
    return sceClibStrnlen(text, 0x7fffffff);
}

int strcmp(const char* left, const char* right) {
    return sceClibStrcmp(left, right);
}

int strncmp(const char* left, const char* right, size_t size) {
    return sceClibStrncmp(left, right, size);
}

char* strchr(const char* text, int value) {
    return sceClibStrchr(text, value);
}

char* strstr(const char* text, const char* needle) {
    return sceClibStrstr(text, needle);
}

void* malloc(size_t size) {
    extern void* sce_paf_malloc(size_t);
    return sce_paf_malloc(size);
}

void* calloc(size_t count, size_t size) {
    extern void* sce_paf_calloc(size_t, size_t);
    return sce_paf_calloc(count, size);
}

void* realloc(void* memory, size_t size) {
    extern void* sce_paf_realloc(void*, size_t);
    return sce_paf_realloc(memory, size);
}

void free(void* memory) {
    extern void sce_paf_free(void*);
    sce_paf_free(memory);
}

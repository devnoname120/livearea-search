#include "paf_compat.h"

#include <cstddef>

void* operator new(std::size_t size) {
    return sce_paf_malloc(size);
}

void* operator new[](std::size_t size) {
    return sce_paf_malloc(size);
}

void operator delete(void* memory) noexcept {
    sce_paf_free(memory);
}

void operator delete[](void* memory) noexcept {
    sce_paf_free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    sce_paf_free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    sce_paf_free(memory);
}

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

extern "C" void __cxa_pure_virtual() {
    for (;;) {
    }
}

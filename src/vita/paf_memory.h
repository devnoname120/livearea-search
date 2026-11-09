#pragma once

#include <cstddef>
#include <new>

#include "paf_compat.h"

namespace livearea_search {

template <typename T, typename... Arguments>
T* PafNew(Arguments&&... arguments) {
    void* memory = sce_paf_malloc(sizeof(T));
    if (memory == NULL) {
        return NULL;
    }
    return ::new (memory) T(static_cast<Arguments&&>(arguments)...);
}

} // namespace livearea_search

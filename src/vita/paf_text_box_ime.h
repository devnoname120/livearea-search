#pragma once

#include "paf_compat.h"

namespace livearea_search {
namespace paf_text_box_ime {

static_assert(sizeof(paf::ui::ImeParam) == 24, "Unexpected PAF IME parameter ABI");

extern "C" int GetImeParamExport(
    paf::ui::TextBox* text_box,
    paf::ui::ImeParam& param)
    __asm__("_ZN3paf2ui7TextBox11GetImeParamERNS1_8ImeParamE");

extern "C" int SetImeParamExport(
    paf::ui::TextBox* text_box,
    const paf::ui::ImeParam& param)
    __asm__("_ZN3paf2ui7TextBox11SetImeParamERKNS1_8ImeParamE");

inline int GetImeParam(paf::ui::TextBox* text_box, paf::ui::ImeParam& param) {
    return GetImeParamExport(text_box, param);
}

inline int SetImeParam(
    paf::ui::TextBox* text_box,
    const paf::ui::ImeParam& param) {
    return SetImeParamExport(text_box, param);
}

} // namespace paf_text_box_ime
} // namespace livearea_search

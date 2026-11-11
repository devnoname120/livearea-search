#pragma once

#include <cstddef>

#include "paf_compat.h"

namespace livearea_search {
namespace paf_pad_device_handler {

typedef void(*DeviceHandler)(paf::inputdevice::pad::Data*);

static_assert(sizeof(paf::inputdevice::pad::Data) == 20,
              "Unexpected PAF pad data ABI");
static_assert(offsetof(paf::inputdevice::pad::Data, paddata) == 8,
              "Unexpected PAF pad button offset");

extern "C" DeviceHandler SetDeviceHandlerExport(DeviceHandler handler)
    __asm__("_ZN3paf11inputdevice3pad16SetDeviceHandlerEPFvPNS1_4DataEE");

inline DeviceHandler SetDeviceHandler(DeviceHandler handler) {
    return SetDeviceHandlerExport(handler);
}

} // namespace paf_pad_device_handler
} // namespace livearea_search

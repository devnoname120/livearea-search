#pragma once

#include <cstddef>
#include <cstdint>

namespace paf_touchscreen_device_handler {

struct DataRecord {
    std::int32_t unknown_00;
    std::int32_t unknown_04;
    std::int32_t x;
    std::int32_t y;
    std::int32_t force;
    std::int32_t state;
    std::int32_t center_x;
    std::int32_t center_y;
};

struct DataList {
    DataRecord records[8];
};

typedef void(*DeviceHandler)(DataList* data);

extern "C" DeviceHandler SetDeviceHandlerImport(
    std::uint32_t port,
    DeviceHandler handler)
    __asm__("_ZN3paf11inputdevice11touchscreen16SetDeviceHandlerEjPFvPNS1_8DataListEE");

inline DeviceHandler SetDeviceHandler(
    std::uint32_t port,
    DeviceHandler handler) {
    return SetDeviceHandlerImport(port, handler);
}

static_assert(sizeof(DataRecord) == 0x20,
    "PAF touchscreen record ABI mismatch");
static_assert(offsetof(DataRecord, x) == 0x08,
    "PAF touchscreen x offset mismatch");
static_assert(offsetof(DataRecord, y) == 0x0C,
    "PAF touchscreen y offset mismatch");
static_assert(offsetof(DataRecord, state) == 0x14,
    "PAF touchscreen state offset mismatch");
static_assert(sizeof(DataList) == 0x100,
    "PAF touchscreen list ABI mismatch");

} // namespace paf_touchscreen_device_handler

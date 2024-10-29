#include "bus_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <fmt/core.h>
#include <optional>

namespace sega {

namespace {

constexpr AddressType kAddressMask = 0xFFFFFF;

bool range_contains(const BusDevice::Range& range, AddressType addr) {
  return range.begin <= addr && addr <= range.end;
}

} // namespace

void BusDevice::add_device(Range range, Device* device) {
  mapped_devices_.emplace_back(range, device);
}

std::optional<Error> BusDevice::read(AddressType addr, MutableDataView data) {
  addr &= kAddressMask;
  if (auto* mapped_device = find_by_addr(addr)) {
    return mapped_device->device->read(addr, data);
  }
  return Error{Error::UnmappedRead, fmt::format("unmapped read address: {:06x} size: {:x}", addr, data.size())};
}

std::optional<Error> BusDevice::write(AddressType addr, DataView data) {
  addr &= kAddressMask;
  if (auto* mapped_device = find_by_addr(addr)) {
    return mapped_device->device->write(addr, data);
  }
  return Error{Error::UnmappedWrite, fmt::format("unmapped write address: {:06x} size: {:x}", addr, data.size())};
}

BusDevice::MappedDevice* BusDevice::find_by_addr(AddressType addr) {
  for (auto& record : mapped_devices_) {
    if (range_contains(record.range, addr)) {
      return &record;
    }
  }
  return nullptr;
}

} // namespace sega

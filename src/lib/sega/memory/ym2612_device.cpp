#include "ym2612_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "spdlog/spdlog.h"
#include <optional>

namespace sega {

std::optional<Error> Ym2612Device::read(AddressType addr, MutableDataView data) {
  spdlog::debug("write to YM2612 device address: {:06x} size: {}", addr, data.size());
  for (auto& value : data) {
    value = 0;
  }
  return std::nullopt;
}

std::optional<Error> Ym2612Device::write(AddressType addr, DataView data) {
  spdlog::debug("write to YM2612 device address: {:06x} byte: {:02x}", addr, data.as<Byte>());
  return std::nullopt;
}

} // namespace sega

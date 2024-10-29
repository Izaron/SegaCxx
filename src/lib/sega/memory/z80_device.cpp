#include "z80_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include <cstddef>
#include <fmt/core.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace sega {

namespace {

constexpr AddressType kRamSize = 0x2000;
constexpr AddressType kZ80BusRequest = 0xA11100;
constexpr AddressType kZ80Reset = 0xA11200;

} // namespace

Z80RamDevice::Z80RamDevice() {
  ram_data_.resize(kRamSize);
}

std::optional<Error> Z80RamDevice::read(AddressType addr, MutableDataView data) {
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = ram_data_[addr + i - kBegin];
  }
  return std::nullopt;
}

std::optional<Error> Z80RamDevice::write(AddressType addr, DataView data) {
  for (size_t i = 0; i < data.size(); ++i) {
    ram_data_[addr + i - kBegin] = data[i];
  }
  return std::nullopt;
}

std::optional<Error> Z80ControllerDevice::read(AddressType addr, MutableDataView data) {
  if (data.size() == 2 && addr == kZ80BusRequest) {
    spdlog::debug("Z80 bus request read: {:04x}", bus_value_);
    data[0] = bus_value_ >> 8;
    data[1] = bus_value_ & 0xFF;
    return std::nullopt;
  }
  // a single byte is fine too
  if (data.size() == 1 && addr == kZ80BusRequest) {
    spdlog::debug("Z80 bus request read: {:02x}", bus_value_ >> 8);
    data[0] = bus_value_ >> 8;
    return std::nullopt;
  }
  return Error{Error::UnmappedRead,
               fmt::format("Unmapped z80 controller read address: {:06x} size: {:x}", addr, data.size())};
}

std::optional<Error> Z80ControllerDevice::write(AddressType addr, DataView data) {
  if (data.size() == 2 && addr == kZ80BusRequest) {
    bus_value_ = data.as<Word>();
    spdlog::debug("Z80 bus request write: {:04x}", bus_value_);
    bus_value_ = bus_value_ == 0x100 ? 0x000 : 0x100; // not a bug
    return std::nullopt;
  }
  if (data.size() == 2 && addr == kZ80Reset) {
    spdlog::debug("Z80 reset write: {:04x}", data.as<Word>());
    return std::nullopt;
  }
  return Error{Error::UnmappedWrite,
               fmt::format("Unmapped z80 controller write address: {:06x} size: {:x}", addr, data.size())};
}

} // namespace sega

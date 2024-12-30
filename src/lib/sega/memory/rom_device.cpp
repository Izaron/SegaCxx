#include "rom_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include <cstddef>
#include <optional>

namespace sega {

RomDevice::RomDevice(DataView rom_data) : rom_data_{rom_data} {}

std::optional<Error> RomDevice::read(AddressType addr, MutableDataView data) {
  for (size_t i = 0; i < data.size() && addr + i < rom_data_.size(); ++i) {
    data[i] = rom_data_[addr + i];
  }
  return std::nullopt;
}

} // namespace sega

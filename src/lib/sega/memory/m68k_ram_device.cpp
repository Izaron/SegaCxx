#include "m68k_ram_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include <cstddef>
#include <optional>

namespace sega {

M68kRamDevice::M68kRamDevice() {
  data_.resize(kEnd - kBegin + 1);
}

std::optional<Error> M68kRamDevice::read(AddressType addr, MutableDataView data) {
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = data_[addr - kBegin + i];
  }
  return std::nullopt;
}

std::optional<Error> M68kRamDevice::write(AddressType addr, DataView data) {
  for (size_t i = 0; i < data.size(); ++i) {
    data_[addr - kBegin + i] = data[i];
  }
  return std::nullopt;
}

} // namespace sega

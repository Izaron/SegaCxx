#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>
#include <vector>

namespace sega {

class M68kRamDevice : public Device {
public:
  static constexpr AddressType kBegin = 0xC00020;
  static constexpr AddressType kEnd = 0xFFFFFF;

  M68kRamDevice();

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;
  std::optional<Error> write(AddressType addr, DataView data) override;

private:
  std::vector<Byte> data_;
};

} // namespace sega

#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>

namespace sega {

class Ym2612Device : public Device {
public:
  static constexpr AddressType kBegin = 0xA04000;
  static constexpr AddressType kEnd = 0xA04003;

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;
  std::optional<Error> write(AddressType addr, DataView data) override;
};

} // namespace sega

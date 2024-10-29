#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>

namespace sega {

class PsgDevice : public WriteOnlyDevice {
public:
  static constexpr AddressType kBegin = 0xC00011;
  static constexpr AddressType kEnd = 0xC00012;

private:
  std::optional<Error> write(AddressType addr, DataView data) override;
};

} // namespace sega

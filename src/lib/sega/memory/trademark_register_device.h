#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>

namespace sega {

class TrademarkRegisterDevice : public WriteOnlyDevice {
public:
  static constexpr AddressType kBegin = 0xA14000;
  static constexpr AddressType kEnd = 0xA14003;

private:
  std::optional<Error> write(AddressType addr, DataView data) override;
};

} // namespace sega

#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>

namespace sega {

class SramAccessRegisterDevice : public WriteOnlyDevice {
public:
  static constexpr AddressType kBegin = 0xA130F1;
  static constexpr AddressType kEnd = 0xA130F1;

private:
  std::optional<Error> write(AddressType addr, DataView data) override;
};

} // namespace sega

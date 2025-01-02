#include "sram_access_register_device.h"
#include "fmt/format.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "spdlog/spdlog.h"
#include <optional>

namespace sega {

std::optional<Error> SramAccessRegisterDevice::write(AddressType addr, DataView data) {
  if (data.size() != 1) {
    return Error{Error::InvalidWrite, fmt::format("Invalid write size: {:x}", data.size())};
  }
  spdlog::debug("SRAM access register written");
  return std::nullopt;
}

} // namespace sega

#include "trademark_register_device.h"
#include "fmt/format.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include <fmt/core.h>
#include <optional>
#include <spdlog/spdlog.h>

namespace sega {

namespace {

constexpr Long kValue = 'SEGA';

}

std::optional<Error> TrademarkRegisterDevice::write(AddressType addr, DataView data) {
  if (data.size() != 4) {
    return Error{Error::InvalidWrite, fmt::format("Invalid write size: {:x}", data.size())};
  }
  const auto value = data.as<Long>();
  if (value != kValue) {
    return Error{Error::InvalidWrite, fmt::format("Invalid write value: {:04x}", value)};
  }
  spdlog::debug("trademark activated");
  return std::nullopt;
}

} // namespace sega

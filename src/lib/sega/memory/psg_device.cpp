#include "psg_device.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include <optional>
#include <spdlog/spdlog.h>

namespace sega {

std::optional<Error> PsgDevice::write(AddressType addr, DataView data) {
  spdlog::debug("write to PSG device byte: {:02x}", data.as<Byte>());
  return std::nullopt;
}

} // namespace sega

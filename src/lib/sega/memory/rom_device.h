#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>

namespace sega {

class RomDevice : public ReadOnlyDevice {
public:
  RomDevice(DataView rom_data);

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;

private:
  DataView rom_data_;
};

} // namespace sega

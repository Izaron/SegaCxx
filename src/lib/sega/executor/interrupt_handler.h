#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/registers/registers.h"
#include "lib/sega/memory/vdp_device.h"
#include <chrono>
#include <optional>

namespace sega {

class InterruptHandler {
public:
  InterruptHandler(AddressType vblank_pc, m68k::Registers& registers, Device& bus_device,
                   const VdpDevice& vdp_device);

  // returns true if an interrupt created
  [[nodiscard]] std::expected<bool, Error> check();

  void reset_time();

private:
  [[nodiscard]] std::optional<Error> call_vblank();

private:
  const AddressType vblank_pc_;
  m68k::Registers& registers_;
  Device& bus_device_;
  const VdpDevice& vdp_device_;

  std::chrono::time_point<std::chrono::steady_clock> prev_fire_{};
};

} // namespace sega

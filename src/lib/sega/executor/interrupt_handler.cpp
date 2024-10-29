#include "interrupt_handler.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/registers/registers.h"
#include "lib/sega/memory/vdp_device.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <spdlog/spdlog.h>
#include <utility>

namespace sega {

using namespace std::literals;

namespace {

constexpr uint8_t VBLANK_INTERRUPT_LEVEL = 6;
constexpr uint8_t HBLANK_INTERRUPT_LEVEL = 4;

constexpr auto NTSC_WAIT_TIME = 0.01666666666s; // 60 frames per second

} // namespace

InterruptHandler::InterruptHandler(AddressType vblank_pc, m68k::Registers& registers, Device& bus_device,
                                   const VdpDevice& vdp_device)
    : vblank_pc_{vblank_pc}, registers_{registers}, bus_device_{bus_device}, vdp_device_{vdp_device} {}

std::expected<bool, Error> InterruptHandler::check() {
  // check only VBLANK now
  if (!vdp_device_.vblank_interrupt_enabled()) {
    return false;
  }
  if (registers_.sr.interrupt_mask >= VBLANK_INTERRUPT_LEVEL) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  if ((now - prev_fire_) >= NTSC_WAIT_TIME) {
    prev_fire_ = now;
    if (auto err = call_vblank()) {
      return std::unexpected(*err);
    }
    return true;
  }

  return false;
}

void InterruptHandler::reset_time() {
  prev_fire_ = std::chrono::steady_clock::now();
}

std::optional<Error> InterruptHandler::call_vblank() {
  // push PC (4 bytes)
  auto& sp = registers_.stack_ptr();
  sp -= 4;
  if (auto err = bus_device_.write(sp, registers_.pc)) {
    return err;
  }

  // push SR (2 bytes)
  sp -= 2;
  if (auto err = bus_device_.write(sp, Word{registers_.sr})) {
    return err;
  }

  // make supervisor, set priority mask, jump to VBLANK
  registers_.sr.supervisor = 1;
  registers_.sr.interrupt_mask = VBLANK_INTERRUPT_LEVEL;
  registers_.pc = vblank_pc_;

  return std::nullopt;
}

} // namespace sega

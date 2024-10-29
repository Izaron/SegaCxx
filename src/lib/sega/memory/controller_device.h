#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include "magic_enum/magic_enum.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace sega {

class ControllerDevice : public ReadOnlyDevice {
public:
  static constexpr AddressType kBegin = 0xA10001;
  static constexpr AddressType kEnd = 0xA1000D;

  enum class Button : uint8_t {
    Up = 0,
    Down = 1,
    Left = 2,
    Right = 3,
    A = 4,
    B = 5,
    C = 6,
    Start = 7,
  };

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;
  std::optional<Error> write(AddressType addr, DataView data) override;

  Byte read_version();
  Byte read_pressed_status(size_t controller);

private:
  static constexpr size_t kControllersCount = 3;
  static constexpr size_t kButtonCount = magic_enum::enum_count<Button>();
  using PressedMap = std::array<bool, kButtonCount>;

  enum class StepNumber {
    Step1,
    Step2,
  };

private:
  std::array<PressedMap, kControllersCount> pressed_map_by_controller_{};
  std::array<StepNumber, kControllersCount> current_step_by_controller_{};
  std::array<Byte, kControllersCount> ctrl_value_{};
};

} // namespace sega

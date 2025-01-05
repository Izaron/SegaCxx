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
  static constexpr AddressType kEnd = 0xA1001F;

  enum class Button : uint8_t {
    // arrow buttons
    Up,
    Down,
    Left,
    Right,

    // 3-controller buttons
    A,
    B,
    C,
    Start,

    // 6-controller buttons
    X,
    Y,
    Z,
    Mode,
  };

  // only for 0th controller currently
  void set_button(Button button, bool pressed);

  void on_vblank();

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
    Step0,
    Step1,
    Step2,
    Step3,
    Step4,
    Step5,
    Step6,
    Step7,
    Step8,
  };

private:
  std::array<PressedMap, kControllersCount> pressed_map_by_controller_{};
  std::array<StepNumber, kControllersCount> current_step_by_controller_{};
  std::array<Byte, kControllersCount> ctrl_value_{};
};

} // namespace sega

#pragma once
#include "lib/common/memory/types.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace sega {

class Colors {
public:
  static constexpr size_t kPaletteCount = 4;
  static constexpr size_t kColorCount = 16;

  struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };
  using Palette = std::array<Color, kColorCount>;

public:
  void update(DataView cram);

  const Palette& palette(size_t palette_idx) const {
    return colors_[palette_idx];
  }

  const Color& color(size_t palette_idx, size_t color_idx) const {
    return colors_[palette_idx][color_idx];
  }

private:
  std::array<Palette, kPaletteCount> colors_;
};

} // namespace sega

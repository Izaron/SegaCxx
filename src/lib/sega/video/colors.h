#pragma once
#include "imgui.h"
#include "lib/common/memory/types.h"
#include <array>
#include <cstddef>

namespace sega {

class Colors {
public:
  static constexpr size_t kPaletteCount = 4;
  static constexpr size_t kColorCount = 16;
  using Palette = std::array<ImVec4, kColorCount>;

public:
  void update(DataView cram);

  const Palette& palette(size_t palette_idx) const {
    return colors_[palette_idx];
  }

  const ImVec4& color(size_t palette_idx, size_t color_idx) const {
    return colors_[palette_idx][color_idx];
  }

private:
  std::array<Palette, kPaletteCount> colors_;
};

} // namespace sega

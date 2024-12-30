#include "colors.h"
#include "lib/common/memory/types.h"
#include "lib/common/util/unreachable.h"
#include <cstddef>
#include <cstdint>

namespace sega {

namespace {

Colors::Color make_color(Word value) {
  // Sega colors can have one value from [0, 2, 4, 6, 8, A, C, E]
  constexpr auto convert = [](auto value) -> uint8_t {
    switch (value / 2) {
    case 0x0:
      return 0;
    case 0x1:
      return 52;
    case 0x2:
      return 87;
    case 0x3:
      return 116;
    case 0x4:
      return 144;
    case 0x5:
      return 172;
    case 0x6:
      return 206;
    case 0x7:
      return 255;
    default:
      unreachable();
    }
  };
  const auto blue = convert((value & 0x0F00) >> 8);
  const auto green = convert((value & 0x00F0) >> 4);
  const auto red = convert(value & 0x000F);
  return {red, green, blue};
}

} // namespace

void Colors::update(DataView cram) {
  for (size_t palette_idx = 0; palette_idx < kPaletteCount; ++palette_idx) {
    for (size_t color_idx = 0; color_idx < kColorCount; ++color_idx) {
      const auto cram_ptr = palette_idx * 32 + color_idx * 2;
      auto& color = colors_[palette_idx][color_idx];
      color = make_color((cram[cram_ptr] << 8) | cram[cram_ptr + 1]);
    }
  }
}

} // namespace sega

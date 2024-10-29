#include "colors.h"
#include "imgui.h"
#include "lib/common/memory/types.h"
#include <cstddef>

namespace sega {

namespace {

ImVec4 make_cram_color(Word value, bool is_background) {
  // Sega colors can have one value from [0, 2, 4, 6, 8, A, C, E]
  constexpr auto convert = [](auto value) -> float { return static_cast<float>(value) / 0xE; };
  const auto blue = convert((value & 0x0F00) >> 8);
  const auto green = convert((value & 0x00F0) >> 4);
  const auto red = convert(value & 0x000F);
  return {red, green, blue, is_background ? 0.75f : 1.0f};
}

} // namespace

void Colors::update(DataView cram) {
  for (size_t palette_idx = 0; palette_idx < kPaletteCount; ++palette_idx) {
    for (size_t color_idx = 0; color_idx < kColorCount; ++color_idx) {
      const auto cram_ptr = palette_idx * 32 + color_idx * 2;
      auto& color = colors_[palette_idx][color_idx];
      color = make_cram_color((cram[cram_ptr] << 8) | cram[cram_ptr + 1], /*is_background=*/color_idx == 0);
    }
  }
}

} // namespace sega

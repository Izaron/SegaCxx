#pragma once
#include "SDL_opengl.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sega {

struct Sprite {
  uint16_t x_coord;
  uint16_t y_coord;
  uint16_t tile_id;
  uint8_t width;
  uint8_t height;
  uint8_t palette;
  uint8_t priority;
  bool flip_horizontally;
  bool flip_vertically;
};

class SpriteTable {
public:
  SpriteTable(const VdpDevice& vdp_device, const Colors& colors);

  std::span<const Sprite> read_sprites();
  std::span<const ImTextureID> draw_sprites(); // call it after `read_sprites`

private:
  static constexpr size_t kMaxSprites = 80;

  const VdpDevice& vdp_device_;
  const Colors& colors_;

  std::array<Sprite, kMaxSprites> sprites_{};
  size_t sprites_count_{};

  std::array<GLuint, kMaxSprites> textures_{};
  std::array<std::vector<uint8_t>, kMaxSprites> canvases_{};
  size_t textures_count_{};
};

} // namespace sega

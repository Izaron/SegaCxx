#pragma once
#include "SDL_opengl.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include "lib/sega/video/sprite_table.h"
#include <cstdint>
#include <span>
#include <vector>

namespace sega {

class Video {
public:
  Video(const VdpDevice& vdp_device);

  std::span<const uint8_t> update();
  ImTextureID draw();

  uint8_t width() const {
    return width_;
  }
  uint8_t height() const {
    return height_;
  }

  const Colors& colors() const {
    return colors_;
  }
  SpriteTable& sprite_table() {
    return sprite_table_;
  }

private:
  void check_size();

private:
  const VdpDevice& vdp_device_;
  Colors colors_;
  SpriteTable sprite_table_;

  uint8_t width_{};  // in tiles
  uint8_t height_{}; // in tiles
  std::vector<uint8_t> canvas_;

  GLuint texture_{};
};

} // namespace sega

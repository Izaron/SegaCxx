#pragma once
#include "SDL_opengl.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sega {

// OpenGL tilemap drawer
class Tilemap {
public:
  Tilemap(const VdpDevice& vdp_device);
  ImTextureID draw(const Colors::Palette& palette);

  uint8_t width() const {
    return width_;
  }
  uint8_t height() const {
    return height_;
  }

private:
  const VdpDevice& vdp_device_;
  GLuint texture_{};
  uint8_t width_{};  // in tiles
  uint8_t height_{}; // in tiles
  std::vector<uint8_t> canvas_;
};

} // namespace sega

#pragma once
#include "SDL_opengl.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vector>

namespace sega {

inline constexpr size_t kPlaneTypes = 3;

enum class PlaneType {
  PlaneA,
  PlaneB,
  Window,
};

struct NametableEntry {
  // byte 1
  uint8_t tile_id_high : 3;
  bool flip_horizontally : 1;
  bool flip_vertically : 1;
  uint8_t palette : 2;
  uint8_t priority : 1;

  // byte 2
  uint8_t tile_id_low;
};
static_assert(sizeof(NametableEntry) == 2);

class Plane {
public:
  Plane(const VdpDevice& vdp_device, PlaneType type);
  ImTextureID draw(const Colors& colors);

  uint8_t width() const {
    return width_;
  }
  uint8_t height() const {
    return height_;
  }

private:
  const VdpDevice& vdp_device_;
  const PlaneType type_;
  GLuint texture_{};
  uint8_t width_{};  // in tiles
  uint8_t height_{}; // in tiles
  std::vector<uint8_t> canvas_;
};

} // namespace sega

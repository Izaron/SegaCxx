#include "plane.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include "lib/sega/video/constants.h"
#include <GL/gl.h>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace sega {

Plane::Plane(const VdpDevice& vdp_device, PlaneType type) : vdp_device_{vdp_device}, type_{type} {
  canvas_.resize(kMaxVpdTiles * kTileSize * kBytesPerPixel);
}

ImTextureID Plane::draw(const Colors& colors) {
  uint8_t cur_width = vdp_device_.plane_width();
  uint8_t cur_height = vdp_device_.plane_height();
  if (width_ != cur_width || height_ != cur_height || !texture_) {
    width_ = cur_width;
    height_ = cur_height;

    // free old texture if present
    if (texture_) {
      glDeleteTextures(1, &texture_);
    }

    // alloc new texture if non-zero
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_ * 8, height_ * 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  // draw image
  const auto table_address = std::invoke([&] {
    switch (type_) {
    case PlaneType::PlaneA:
      return vdp_device_.plane_a_table_address();
    case PlaneType::PlaneB:
      return vdp_device_.plane_b_table_address();
    case PlaneType::Window:
      return vdp_device_.window_table_address();
    }
  });
  for (size_t j = 0; j < height_; ++j) {
    for (size_t i = 0; i < width_; ++i) {
      const auto* nametable_vram_ptr =
          vdp_device_.vram_data().data() + table_address + sizeof(NametableEntry) * (j * width_ + i);
      const auto& nametable_entry = *reinterpret_cast<const NametableEntry*>(nametable_vram_ptr);

      const auto tile_idx = (nametable_entry.tile_id_high << 8) | nametable_entry.tile_id_low;
      const auto* vram_ptr = vdp_device_.vram_data().data() + kVramBytesPerTile * tile_idx;

      for (size_t tile_j = 0; tile_j < kTileDimension; ++tile_j) {
        for (size_t tile_i = 0; tile_i < kTileDimension; ++tile_i) {
          const auto pixel_i =
              i * kTileDimension + (nametable_entry.flip_horizontally ? (kTileDimension - tile_i - 1) : tile_i);
          const auto pixel_j =
              j * kTileDimension + (nametable_entry.flip_vertically ? (kTileDimension - tile_j - 1) : tile_j);
          const auto pixel_idx = pixel_j * (kTileDimension * width_) + pixel_i;
          auto* canvas_ptr = canvas_.data() + kBytesPerPixel * pixel_idx;

          uint8_t cram_color;
          if (tile_i % 2 == 0) {
            cram_color = (*vram_ptr & 0xF0) >> 4;
          } else {
            cram_color = *vram_ptr++ & 0xF;
          }

          if (cram_color == 0) {
            // transparent color
            *canvas_ptr++ = 0;
            *canvas_ptr++ = 0;
            *canvas_ptr++ = 0;
            *canvas_ptr++ = 0;
          } else {
            // color from palette
            const auto& color = colors.color(nametable_entry.palette, cram_color);
            *canvas_ptr++ = color.red;
            *canvas_ptr++ = color.green;
            *canvas_ptr++ = color.blue;
            *canvas_ptr++ = 255;
          }
        }
      }
    }
  }

  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_ * 8, height_ * 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas_.data());
  return texture_;
}

} // namespace sega

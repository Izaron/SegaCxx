#include "tilemap.h"
#include "SDL_opengl.h"
#include "constants.h"
#include "imgui.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include <cstddef>
#include <cstdint>

namespace sega {

Tilemap::Tilemap(const VdpDevice& vdp_device) : vdp_device_{vdp_device} {
  canvas_.resize(kMaxVpdTiles * kTileSize * kBytesPerPixel);
}

ImTextureID Tilemap::draw(const Colors::Palette& palette) {
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
  for (size_t j = 0; j < height_; ++j) {
    for (size_t i = 0; i < width_; ++i) {
      const auto tile_idx = j * width_ + i;
      const auto* vram_ptr = vdp_device_.vram_data().data() + kVramBytesPerTile * tile_idx;

      for (size_t tile_j = 0; tile_j < kTileDimension; ++tile_j) {
        for (size_t tile_i = 0; tile_i < kTileDimension; ++tile_i) {
          const auto pixel_i = i * kTileDimension + tile_i;
          const auto pixel_j = j * kTileDimension + tile_j;
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
            const auto& color = palette[cram_color];
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

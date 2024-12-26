#include "sprite_table.h"
#include "SDL_opengl.h"
#include "constants.h"
#include "imgui.h"
#include "lib/common/memory/types.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/colors.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace sega {

namespace {

struct SpriteEntry {
  // bytes 1-2
  BigEndian<uint16_t> y_coord;

  // byte 3
  uint8_t height : 2; // in tiles minus one
  uint8_t width : 2;  // in tiles minus one
  uint8_t _ : 4;

  // byte 4
  uint8_t sprite_link;

  // byte 5
  uint8_t tile_id_high : 3;
  bool flip_horizontally : 1;
  bool flip_vertically : 1;
  uint8_t palette : 2;
  uint8_t priority : 1;

  // byte 6
  uint8_t tile_id_low;

  // bytes 7-8
  BigEndian<uint16_t> x_coord;
};
static_assert(sizeof(SpriteEntry) == 8);

} // namespace

SpriteTable::SpriteTable(const VdpDevice& vdp_device, const Colors& colors) : vdp_device_{vdp_device}, colors_{colors} {
  for (auto& canvas : canvases_) {
    canvas.resize(kMaxSpriteTiles * kTileSize * kBytesPerPixel);
  }
}

std::span<const Sprite> SpriteTable::read_sprites() {
  const Word base_addr = vdp_device_.sprite_table_address();
  uint8_t sprite_id = 0;
  sprites_count_ = 0;
  while (true) {
    const auto& sprite_entry = *reinterpret_cast<const SpriteEntry*>(vdp_device_.vram_data().data() + base_addr +
                                                                     sprite_id * sizeof(SpriteEntry));

    sprites_[sprites_count_++] = Sprite{
        .x_coord = sprite_entry.x_coord.get(),
        .y_coord = sprite_entry.y_coord.get(),
        .tile_id = static_cast<uint16_t>((sprite_entry.tile_id_high << 8) + sprite_entry.tile_id_low),
        .width = static_cast<uint8_t>(sprite_entry.width + 1),
        .height = static_cast<uint8_t>(sprite_entry.height + 1),
        .palette = sprite_entry.palette,
        .priority = sprite_entry.priority,
        .flip_horizontally = sprite_entry.flip_horizontally,
        .flip_vertically = sprite_entry.flip_vertically,
    };

    if ((sprite_id = sprite_entry.sprite_link) == 0) {
      break;
    }
  }

  return {sprites_.data(), sprites_count_};
}

std::span<const ImTextureID> SpriteTable::draw_sprites() {
  // free old textures
  glDeleteTextures(static_cast<GLsizei>(textures_count_), textures_.data());
  for (size_t sprite_idx = 0; sprite_idx < sprites_count_; ++sprite_idx) {
    const auto& sprite = sprites_[sprite_idx];
    auto& canvas = canvases_[sprite_idx];
    auto& texture = textures_[sprite_idx];

    // draw sprite to canvas
    for (size_t i = 0; i < sprite.width; ++i) {
      for (size_t j = 0; j < sprite.height; ++j) {
        const auto tile_idx = sprite.tile_id + i * sprite.height + j;
        const auto* vram_ptr = vdp_device_.vram_data().data() + kVramBytesPerTile * tile_idx;

        for (size_t tile_j = 0; tile_j < kTileDimension; ++tile_j) {
          for (size_t tile_i = 0; tile_i < kTileDimension; ++tile_i) {
            const auto pixel_i = i * kTileDimension + tile_i;
            const auto pixel_j = j * kTileDimension + tile_j;
            const auto pixel_idx = pixel_j * (kTileDimension * sprite.width) + pixel_i;
            auto* canvas_ptr = canvas.data() + kBytesPerPixel * pixel_idx;

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
              const auto& color = colors_.color(sprite.palette, cram_color);
              *canvas_ptr++ = color.red;
              *canvas_ptr++ = color.green;
              *canvas_ptr++ = color.blue;
              *canvas_ptr++ = 255;
            }
          }
        }
      }
    }

    // alloc new texture and load canvas to it
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sprite.width * 8, sprite.height * 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 canvas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  textures_count_ = sprites_count_;
  return {reinterpret_cast<const ImTextureID*>(textures_.data()), textures_count_};
}

} // namespace sega

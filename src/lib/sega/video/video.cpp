#include "video.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "lib/common/memory/types.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/video/constants.h"
#include "lib/sega/video/plane.h"
#include "spdlog/spdlog.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <span>

namespace sega {

Video::Video(const VdpDevice& vdp_device) : vdp_device_{vdp_device}, sprite_table_{vdp_device_, colors_} {}

std::span<const uint8_t> Video::update() {
  check_size();
  colors_.update(vdp_device_.cram_data());
  const auto sprites = sprite_table_.read_sprites();

  auto* canvas_ptr = canvas_.data();

  const auto try_draw_sprite = [&](size_t x, size_t y, bool priority) -> bool {
    for (const auto& sprite : sprites) {
      if (sprite.priority != priority) {
        continue;
      }

      // calculate sprite box
      size_t left = sprite.x_coord - 128;
      size_t right = left + sprite.width * kTileDimension;
      size_t top = sprite.y_coord - 128;
      size_t bottom = top + sprite.height * kTileDimension;

      // check if the current pixel inside the box
      if ((left <= x && x < right) && (top <= y && y < bottom)) {
        // calculate tile id and pixel coordinate inside it
        size_t x_pos = sprite.flip_horizontally ? (right - x - 1) : (x - left);
        size_t y_pos = sprite.flip_vertically ? (bottom - y - 1) : (y - top);

        size_t tile_x = x_pos / kTileDimension;
        size_t tile_y = y_pos / kTileDimension;
        size_t tile_id = sprite.tile_id + tile_x * sprite.height + tile_y;

        size_t inside_x = x_pos % kTileDimension;
        size_t inside_y = y_pos % kTileDimension;
        size_t pixel_id = inside_y * kTileDimension + inside_x;

        const auto* vram_ptr = vdp_device_.vram_data().data() + kVramBytesPerTile * tile_id;
        const auto vram_byte = *(vram_ptr + pixel_id / 2);
        const uint8_t cram_color = (pixel_id % 2 == 0) ? ((vram_byte & 0xF0) >> 4) : (vram_byte & 0xF);

        if (cram_color != 0) {
          // color from palette
          const auto& color = colors_.palette(sprite.palette)[cram_color];
          *canvas_ptr++ = color.red;
          *canvas_ptr++ = color.green;
          *canvas_ptr++ = color.blue;
          *canvas_ptr++ = 255;
          return true;
        }
      }
    }
    return false;
  };

  const auto try_draw_plane = [&](PlaneType plane_type, size_t x, size_t y, bool priority) -> bool {
    if (plane_type == PlaneType::Window) {
      if (vdp_device_.window_split_mode() == VdpDevice::WindowSplitMode::X) {
        if (vdp_device_.window_display_to_the_right() && x < vdp_device_.window_x_split()) {
          return false;
        }
        if (not vdp_device_.window_display_to_the_right() && x >= vdp_device_.window_x_split()) {
          return false;
        }
      } else {
        if (vdp_device_.window_display_below() && y < vdp_device_.window_y_split()) {
          return false;
        }
        if (not vdp_device_.window_display_below() && y >= vdp_device_.window_y_split()) {
          return false;
        }
      }
    } else {
      // apply horizontal scrolling
      const auto* hscroll_ram_ptr = reinterpret_cast<const BigEndian<Word>*>(vdp_device_.vram_data().data() +
                                                                             vdp_device_.hscroll_table_address());
      switch (vdp_device_.horizontal_scroll_mode()) {
      case VdpDevice::HorizontalScrollMode::FullScroll:
        x -= hscroll_ram_ptr[plane_type == PlaneType::PlaneA ? 0 : 1].get();
        break;
      case VdpDevice::HorizontalScrollMode::ScrollEightLinesThenRepeat:
        std::abort(); // unsupported now, don't understand this mode
        break;
      case VdpDevice::HorizontalScrollMode::ScrollEveryTile:
        x -= hscroll_ram_ptr[(y - (y % 8)) * 2 + (plane_type == PlaneType::PlaneA ? 0 : 1)].get();
        break;
      case VdpDevice::HorizontalScrollMode::ScrollEveryLine:
        x -= hscroll_ram_ptr[y * 2 + (plane_type == PlaneType::PlaneA ? 0 : 1)].get();
        break;
      }

      // apply vertical scrolling
      const auto* vscroll_ram_ptr = reinterpret_cast<const BigEndian<Word>*>(vdp_device_.vsram_data().data());
      switch (vdp_device_.vertical_scroll_mode()) {
      case VdpDevice::VerticalScrollMode::FullScroll:
        y += vscroll_ram_ptr[plane_type == PlaneType::PlaneA ? 0 : 1].get();
        break;
      case VdpDevice::VerticalScrollMode::ScrollEveryTwoTiles:
        y += vscroll_ram_ptr[(y / 16) * 2 + (plane_type == PlaneType::PlaneA ? 0 : 1)].get();
        break;
      }
    }

    const auto table_address = std::invoke([&] {
      switch (plane_type) {
      case PlaneType::PlaneA:
        return vdp_device_.plane_a_table_address();
      case PlaneType::PlaneB:
        return vdp_device_.plane_b_table_address();
      case PlaneType::Window:
        return vdp_device_.window_table_address();
      }
    });

    size_t tile_x = (x / kTileDimension) % vdp_device_.tilemap_width();
    size_t tile_y = (y / kTileDimension) % vdp_device_.tilemap_height();

    const auto* nametable_vram_ptr = vdp_device_.vram_data().data() + table_address +
                                     sizeof(NametableEntry) * (tile_y * vdp_device_.tilemap_width() + tile_x);
    const auto& nametable_entry = *reinterpret_cast<const NametableEntry*>(nametable_vram_ptr);
    if (nametable_entry.priority != priority) {
      return false;
    }

    const auto tile_idx = (nametable_entry.tile_id_high << 8) | nametable_entry.tile_id_low;
    const auto* vram_ptr = vdp_device_.vram_data().data() + kVramBytesPerTile * tile_idx;

    size_t inside_x = x % kTileDimension;
    if (nametable_entry.flip_horizontally) {
      inside_x = 7 - inside_x;
    }
    size_t inside_y = y % kTileDimension;
    if (nametable_entry.flip_vertically) {
      inside_y = 7 - inside_y;
    }
    size_t pixel_id = inside_y * kTileDimension + inside_x;
    const auto vram_byte = *(vram_ptr + pixel_id / 2);
    const uint8_t cram_color = (pixel_id % 2 == 0) ? ((vram_byte & 0xF0) >> 4) : (vram_byte & 0xF);
    if (cram_color != 0) {
      const auto& color = colors_.color(nametable_entry.palette, cram_color);
      *canvas_ptr++ = color.red;
      *canvas_ptr++ = color.green;
      *canvas_ptr++ = color.blue;
      *canvas_ptr++ = 255;
      return true;
    }
    return false;
  };

  // draw each scanline, so iterate from left to right
  for (size_t y = 0; y < height_ * kTileDimension; ++y) {
    for (size_t x = 0; x < width_ * kTileDimension; ++x) {
      const bool result = std::invoke([&] -> bool {
        for (const bool priority : {true, false}) {
          if (try_draw_sprite(x, y, priority)) {
            return true;
          }
          if (try_draw_plane(PlaneType::Window, x, y, priority)) {
            return true;
          }
          if (try_draw_plane(PlaneType::PlaneA, x, y, priority)) {
            return true;
          }
          if (try_draw_plane(PlaneType::PlaneB, x, y, priority)) {
            return true;
          }
        }
        return false;
      });
      if (result) {
        continue;
      }

      // draw the background
      const auto& color = colors_.palette(vdp_device_.background_color_palette())[vdp_device_.background_color_index()];
      *canvas_ptr++ = color.red;
      *canvas_ptr++ = color.green;
      *canvas_ptr++ = color.blue;
      *canvas_ptr++ = 255;
    }
  }

  return canvas_;
}

ImTextureID Video::draw() {
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_ * kTileDimension, height_ * kTileDimension, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, canvas_.data());
  return texture_;
}

void Video::check_size() {
  bool size_changed{};
  if (const auto vdp_width = vdp_device_.tile_width(); vdp_width != width_) {
    width_ = vdp_width;
    size_changed = true;
    spdlog::info("set game width: {}", width_);
  }
  if (const auto vdp_height = vdp_device_.tile_height(); vdp_height != height_) {
    height_ = vdp_height;
    size_changed = true;
    spdlog::info("set game height: {}", height_);
  }
  if (size_changed) {
    // RGBA encoding
    canvas_.resize((kTileDimension * width_) * (kTileDimension * height_) * 4);

    // free old texture if present
    if (texture_) {
      glDeleteTextures(1, &texture_);
    }

    // alloc new texture if non-zero
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_ * kTileDimension, height_ * kTileDimension, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, canvas_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
}

} // namespace sega

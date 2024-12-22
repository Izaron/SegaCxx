#include "image_saver.h"
#include <cstdint>
#include <span>
#include <string_view>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <spdlog/spdlog.h>

namespace sega {

void save_to_png(std::string_view filename, int width, int height, std::span<const uint8_t> data) {
  if (stbi_write_png(filename.data(), width, height, 4, data.data(), width * 4)) {
    spdlog::info("PNG file written: {}", filename);
  } else {
    spdlog::error("failed to write PNG file: {}", filename);
  }
}

} // namespace sega

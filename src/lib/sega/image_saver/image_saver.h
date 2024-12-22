#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace sega {

void save_to_png(std::string_view filename, int width, int height, std::span<const uint8_t> data);

} // namespace sega

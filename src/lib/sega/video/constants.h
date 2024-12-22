#pragma once
#include <cstddef>

namespace sega {

constexpr size_t kMaxVpdTiles = 64ULL * 64;
constexpr size_t kMaxSpriteTiles = 4ULL * 4;
constexpr size_t kTileDimension = 8;
constexpr size_t kTileSize = kTileDimension * kTileDimension;
constexpr size_t kBytesPerPixel = 4;
constexpr size_t kVramBytesPerTile = 32;

} // namespace sega

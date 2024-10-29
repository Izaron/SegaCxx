#pragma once
#include "lib/common/memory/types.h"
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sega {

struct AddressRange {
  BigEndian<uint32_t> begin;
  BigEndian<uint32_t> end;
};
static_assert(sizeof(AddressRange) == 8);

struct VectorTable {
  BigEndian<uint32_t> reset_sp;
  BigEndian<uint32_t> reset_pc;
  std::array<uint32_t, 26> _;
  BigEndian<uint32_t> hblank_pc;
  uint32_t _;
  BigEndian<uint32_t> vblank_pc;
  std::array<uint32_t, 33> _;
};
static_assert(sizeof(VectorTable) == 256);

struct Metadata {
  Bytes<16> system_type;
  Bytes<16> copyright;
  Bytes<48> domestic_title;
  Bytes<48> overseas_title;
  Bytes<14> serial_number;
  BigEndian<uint16_t> checksum;
  Bytes<16> device_support;
  AddressRange rom_address;
  AddressRange ram_address;
  Bytes<12> extra_memory;
  Bytes<12> modem_support;
  Bytes<40> _;
  Bytes<3> region_support;
  Bytes<13> _;
};
static_assert(sizeof(Metadata) == 256);

struct Header {
  VectorTable vector_table;
  Metadata metadata;
};
static_assert(sizeof(Header) == 512);

std::vector<char> load_rom(std::string_view path);

} // namespace sega

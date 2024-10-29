#pragma once
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace m68k {

using Byte = uint8_t;
using Word = uint16_t;
using Long = uint32_t;
using LongLong = uint64_t;

using SignedByte = int8_t;
using SignedWord = int16_t;
using SignedLong = int32_t;
using SignedLongLong = int64_t;

using AddressType = Long;
using AddressRange = std::pair<AddressType, AddressType>;

using MutableDataView = std::span<Byte>;
using DataView = std::span<const Byte>;
using DataHolder = std::vector<Byte>;

} // namespace m68k

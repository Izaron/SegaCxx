#pragma once
#include "fmt/base.h"
#include "fmt/format.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fmt/core.h>
#include <span>
#include <string>

using Byte = uint8_t;
using Word = uint16_t;
using Long = uint32_t;
using LongLong = uint64_t;

using SignedByte = int8_t;
using SignedWord = int16_t;
using SignedLong = int32_t;
using SignedLongLong = int64_t;

template<size_t Size>
using Bytes = std::array<char, Size>;

template<typename T>
class BigEndian {
public:
  T get() const {
    return std::byteswap(value_);
  }

private:
  T value_;
};

using AddressType = Long;

using MutableDataView = std::span<Byte>;

class DataView : public std::span<const Byte> {
public:
  using Base = std::span<const Byte>;
  using Base::Base;

  template<std::integral T>
  T as() {
    return std::byteswap(*reinterpret_cast<const T*>(data()));
  }
};

template<>
struct fmt::formatter<DataView> : formatter<std::string> {
  auto format(DataView data_view, format_context& ctx) const {
    std::string result;
    result.reserve(data_view.size() * 3 + 1);
    result.push_back('[');
    for (size_t i = 0; i < data_view.size(); ++i) {
      if (i > 0) {
        result.push_back(' ');
      }
      const auto write_nibble = [&result](const auto nibble) {
        if (nibble > 9) {
          result.push_back(static_cast<char>(nibble - 10 + 'A'));
        } else {
          result.push_back(static_cast<char>(nibble + '0'));
        }
      };
      write_nibble(data_view[i] >> 4);
      write_nibble(data_view[i] & 0xF);
    }
    result.push_back(']');
    return formatter<std::string>::format(result, ctx);
  }
};

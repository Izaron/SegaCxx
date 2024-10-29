#pragma once
#include <bit>
#include <expected>
#include <optional>
#include <utility>

#include "lib/m68k/error/error.h"
#include "types.h"

namespace m68k {

class Device {
public:
  // reads `data.size()` bytes from address `addr`
  virtual std::optional<Error> read(AddressType addr, MutableDataView data) = 0;

  // writes `data.size()` bytes to address `addr`
  [[nodiscard]] virtual std::optional<Error> write(AddressType addr, DataView data) = 0;

  template<std::integral T>
  std::expected<T, Error> read(AddressType addr) {
    T data;
    if (auto err = read(addr, MutableDataView{reinterpret_cast<Byte*>(&data), sizeof(T)})) {
      return std::unexpected{std::move(*err)};
    }
    // swap bytes after reading to make it little-endian
    return std::byteswap(data);
  }

  template<std::integral T>
  [[nodiscard]] std::optional<Error> write(AddressType addr, T value) {
    // swap bytes before writing to make it big-endian
    const auto swapped = std::byteswap(value);
    return write(addr, DataView{reinterpret_cast<const Byte*>(&swapped), sizeof(T)});
  }
};

} // namespace m68k

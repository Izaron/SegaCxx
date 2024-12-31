#pragma once
#include <bit>
#include <expected>
#include <fmt/core.h>
#include <optional>
#include <utility>

#include "fmt/format.h"
#include "lib/common/error/error.h"
#include "spdlog/spdlog.h"
#include "types.h"

class Device {
public:
  // reads `data.size()` bytes from address `addr`
  [[nodiscard]] virtual std::optional<Error> read(AddressType addr, MutableDataView data) = 0;

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

class ReadOnlyDevice : public Device {
private:
  std::optional<Error> write(AddressType addr, DataView data) override {
    // some games do it, just print an error
    spdlog::error("protected write address: {:06x} size: {:x}", addr, data.size());
    return std::nullopt;
  }
};

class WriteOnlyDevice : public Device {
private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override {
    return Error{Error::ProtectedRead,
                       fmt::format("protected read address: {:06x} size: {:x}", addr, data.size())};
  }
};

class DummyDevice final : public Device {
private:
  std::optional<Error> write(AddressType addr, DataView data) override {
    return Error{Error::ProtectedWrite,
                       fmt::format("protected write address: {:06x} size: {:x}", addr, data.size())};
  }

  std::optional<Error> read(AddressType addr, MutableDataView data) override {
    return Error{Error::ProtectedRead,
                       fmt::format("protected read address: {:06x} size: {:x}", addr, data.size())};
  }
};

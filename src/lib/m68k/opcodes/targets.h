#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include "lib/m68k/emulator/emulator.h"
#include "lib/m68k/error/error.h"
#include "lib/m68k/memory/types.h"

namespace m68k {

class Target {
public:
  enum Kind : uint8_t {
    DataRegisterKind,
    AddressRegisterKind,
    AddressKind,
    AddressIncrementKind,
    AddressDecrementKind,
    AddressDisplacementKind,
    AddressIndexKind,
    ProgramCounterDisplacementKind,
    ProgramCounterIndexKind,
    AbsoluteShortKind,
    AbsoluteLongKind,
    ImmediateKind,
  };

  Target& kind(Kind kind);
  Target& size(uint8_t size);
  Target& index(uint8_t index);
  Target& ext_word0(Word extWord0);
  Target& ext_word1(Word extWord1);
  Target& address(Long address);

  // pre-work and post-work
  void set_inc_or_dec_count(std::size_t count);
  void try_decrement_address(Context ctx, std::size_t count = 1);
  void try_increment_address(Context ctx, std::size_t count = 1);

  // helper methods
  Long effective_address(Context ctx) const;
  Kind kind() const {
    return kind_;
  }
  uint8_t index() const {
    return index_;
  }

  // read methods
  std::optional<Error> read(Context ctx, MutableDataView data);
  std::expected<LongLong, Error> read_as_long_long(Context ctx, AddressType size);

  template<std::integral T>
  std::expected<T, Error> read(Context ctx) {
    T data;
    if (auto err = read(ctx, MutableDataView{reinterpret_cast<Byte*>(&data), sizeof(T)})) {
      return std::unexpected{std::move(*err)};
    }
    // swap bytes after reading to make it little-endian
    return std::byteswap(data);
  }

  // write methods
  [[nodiscard]] std::optional<Error> write(Context ctx, DataView data);
  [[nodiscard]] std::optional<Error> write_sized(Context ctx, Long value, AddressType size);

  template<std::integral T>
  [[nodiscard]] std::optional<Error> write(Context ctx, T value) {
    // swap bytes before writing to make it big-endian
    value = std::byteswap(value);
    return write(ctx, {reinterpret_cast<Byte*>(&value), sizeof(T)});
  }

private:
  Long indexed_address(Context ctx, Long baseAddress) const;

private:
  Kind kind_;
  uint8_t size_;
  uint8_t index_;
  Word ext_word0_;
  Word ext_word1_;
  Long address_;

  bool already_decremented_;
  std::size_t inc_or_dec_count_;
};

static_assert(sizeof(Target) == 24);
static_assert(std::is_trivially_constructible_v<Target>);

} // namespace m68k

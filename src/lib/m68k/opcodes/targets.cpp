#include "targets.h"
#include "lib/m68k/emulator/emulator.h"
#include "lib/m68k/error/error.h"
#include "lib/m68k/memory/types.h"
#include "lib/m68k/registers/registers.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace m68k {

namespace {

Long& a_reg(Registers& r, int index) {
  if (index < 7) {
    return r.a[index];
  } else {
    return r.sr.supervisor ? r.ssp : r.usp;
  }
}

int8_t bits_range(auto value, std::size_t begin, std::size_t len) {
  return (value >> begin) & ((1 << len) - 1);
}

int8_t bit_at(auto value, std::size_t bit) {
  return bits_range(value, bit, 1);
}

uint8_t scale_value(int8_t mode) {
  // NOTE: scaled indexing is disabled at basic Motorola 68000
  return 1;

  switch (mode) {
  case 0:
    return 1;
  case 1:
    return 2;
  case 2:
    return 4;
  case 3:
    return 8;
  default:
    std::unreachable();
  }
}

} // namespace

Target& Target::kind(Kind kind) {
  kind_ = kind;
  if (kind_ == AddressDecrementKind) {
    already_decremented_ = false;
  }
  return *this;
}

Target& Target::size(uint8_t size) {
  size_ = size;
  return *this;
}

Target& Target::index(uint8_t index) {
  index_ = index;
  return *this;
}

Target& Target::ext_word0(Word extWord0) {
  ext_word0_ = extWord0;
  return *this;
}

Target& Target::ext_word1(Word extWord1) {
  ext_word1_ = extWord1;
  return *this;
}

Target& Target::address(Long address) {
  address_ = address;
  return *this;
}

void Target::set_inc_or_dec_count(std::size_t count) {
  inc_or_dec_count_ = count;
}

void Target::try_decrement_address(Context ctx, std::size_t count) {
  if (kind_ == AddressDecrementKind && !already_decremented_) {
    auto& reg = a_reg(ctx.registers, index_);

    // stack pointer should be aligned to a word boundary
    Long diff = size_ * count;
    reg -= (index_ == 7) ? std::max(diff, Long{2}) : diff;
  }
  already_decremented_ = true;
}

void Target::try_increment_address(Context ctx, std::size_t count) {
  if (kind_ == AddressIncrementKind) {
    auto& reg = a_reg(ctx.registers, index_);

    // stack pointer should be aligned to a word boundary
    Long diff = size_ * count;
    reg += (index_ == 7) ? std::max(diff, Long{2}) : diff;
  }
}

Long Target::effective_address(Context ctx) const {
  switch (kind_) {
  case AddressKind:
  case AddressIncrementKind:
  case AddressDecrementKind:
    return a_reg(ctx.registers, index_);
  case AddressDisplacementKind:
    return a_reg(ctx.registers, index_) + static_cast<SignedWord>(ext_word0_);
  case AddressIndexKind:
    return indexed_address(ctx, a_reg(ctx.registers, index_));
  case ProgramCounterDisplacementKind:
    return ctx.registers.pc - 2 + static_cast<SignedWord>(ext_word0_);
  case ProgramCounterIndexKind:
    return indexed_address(ctx, ctx.registers.pc - 2);
  case AbsoluteShortKind:
    return static_cast<SignedWord>(ext_word0_);
  case AbsoluteLongKind:
    return (ext_word0_ << 16) + ext_word1_;
  case ImmediateKind:
    return address_;
  default:
    std::unreachable();
  }
}

std::optional<Error> Target::read(Context ctx, MutableDataView data) {
  try_decrement_address(ctx, inc_or_dec_count_);

  const auto read_register = [&data](Long reg) {
    // didn't come up with more clever approach
    for (int i = 0; i < data.size(); ++i) {
      data[i] = reg & 0xFF;
      reg >>= 8;
    }
    std::ranges::reverse(data);
  };

  switch (kind_) {
  case DataRegisterKind:
    read_register(ctx.registers.d[index_]);
    break;
  case AddressRegisterKind:
    read_register(a_reg(ctx.registers, index_));
    break;
  case AbsoluteLongKind:
  case AbsoluteShortKind:
  case AddressDecrementKind:
  case AddressDisplacementKind:
  case AddressIncrementKind:
  case AddressIndexKind:
  case AddressKind:
  case ImmediateKind:
  case ProgramCounterDisplacementKind:
  case ProgramCounterIndexKind:
    if (auto err = ctx.device.read(effective_address(ctx), data)) {
      return std::move(err);
    }
    break;
  }

  return std::nullopt;
}

std::expected<LongLong, Error> Target::read_as_long_long(Context ctx, AddressType size) {
  std::array<Byte, sizeof(LongLong)> data;
  assert(size <= data.size());
  if (auto err = read(ctx, {data.data(), size})) {
    return std::unexpected{std::move(*err)};
  }

  // didn't come up with more clever approach
  LongLong res = data[0];
  for (int i = 1; i < size; ++i) {
    res = (res << 8) + data[i];
  }
  return res;
}

std::optional<Error> Target::write(Context ctx, DataView data) {
  try_decrement_address(ctx, inc_or_dec_count_);

  const auto write_register = [data](Long& reg) {
    Long shift = 0;
    Long lsb = 0;
    for (const auto value : data) {
      shift += 8;
      lsb <<= 8;
      lsb += value;
    }

    if (shift == 32) {
      reg = 0;
    } else {
      reg >>= shift;
      reg <<= shift;
    }
    reg |= lsb;
  };

  switch (kind_) {
  case DataRegisterKind:
    write_register(ctx.registers.d[index_]);
    break;
  case AddressRegisterKind:
    write_register(a_reg(ctx.registers, index_));
    break;
  case AddressKind:
  case AddressIncrementKind:
  case AddressDecrementKind:
    return ctx.device.write(a_reg(ctx.registers, index_), data);
  case AddressDisplacementKind:
    return ctx.device.write(a_reg(ctx.registers, index_) + static_cast<SignedWord>(ext_word0_), data);
  case AddressIndexKind:
    return ctx.device.write(indexed_address(ctx, a_reg(ctx.registers, index_)), data);
  case ProgramCounterDisplacementKind:
    return ctx.device.write(ctx.registers.pc - 2 + static_cast<SignedWord>(ext_word0_), data);
  case ProgramCounterIndexKind:
    return ctx.device.write(indexed_address(ctx, ctx.registers.pc - 2), data);
  case AbsoluteShortKind:
    return ctx.device.write(static_cast<SignedWord>(ext_word0_), data);
  case AbsoluteLongKind:
    return ctx.device.write((ext_word0_ << 16) + ext_word1_, data);
  case ImmediateKind:
    return ctx.device.write(address_, data);
  }

  return std::nullopt;
}

std::optional<Error> Target::write_sized(Context ctx, Long value, AddressType size) {
  switch (size) {
  case 1:
    return write<Byte>(ctx, value);
  case 2:
    return write<Word>(ctx, value);
  case 4:
    return write<Long>(ctx, value);
  default:
    std::unreachable();
  }
}

Long Target::indexed_address(Context ctx, Long baseAddress) const {
  const uint8_t xregNum = bits_range(ext_word0_, 12, 3);
  const Long xreg = bit_at(ext_word0_, 15) ? a_reg(ctx.registers, xregNum) : ctx.registers.d[xregNum];
  const Long size = bit_at(ext_word0_, 11) ? /*Long*/ 4 : /*Word*/ 2;
  const Long scale = scale_value(bits_range(ext_word0_, 9, 2));
  const SignedByte disp = static_cast<SignedByte>(bits_range(ext_word0_, 0, 8));

  SignedLong clarifiedXreg = static_cast<SignedLong>(xreg);
  if (size == 2) {
    clarifiedXreg = static_cast<SignedWord>(clarifiedXreg);
  }

  return baseAddress + disp + clarifiedXreg * scale;
}

} // namespace m68k

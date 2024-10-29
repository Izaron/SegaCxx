#include "instruction.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fmt/core.h>
#include <optional>
#include <utility>

#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/common/context.h"
#include "lib/m68k/registers/registers.h"
#include "lib/m68k/target/target.h"

namespace m68k {

namespace {

enum OpcodeType {
  AddType,
  AndType,
  CmpType,
  EorType,
  OrType,
  SubType,
};

OpcodeType opcode_type(Instruction::Kind kind) {
  switch (kind) {
  case Instruction::AddKind... Instruction::AddxKind:
    return AddType;
  case Instruction::AndKind... Instruction::AndiToSrKind:
    return AndType;
  case Instruction::CmpKind... Instruction::CmpmKind:
    return CmpType;
  case Instruction::EorKind... Instruction::EoriToSrKind:
    return EorType;
  case Instruction::OrKind... Instruction::OriToSrKind:
    return OrType;
  case Instruction::SubKind... Instruction::SubxKind:
    return SubType;
  default:
    std::unreachable();
  }
}

std::integral auto do_binary_op(OpcodeType type, std::integral auto lhs, std::integral auto rhs) {
  switch (type) {
  case AddType:
    return lhs + rhs;
  case AndType:
    return lhs & rhs;
  case EorType:
    return lhs ^ rhs;
  case OrType:
    return lhs | rhs;
  case SubType:
  case CmpType:
    return rhs - lhs;
  default:
    std::unreachable();
  }
}

bool is_substract_op(OpcodeType type) {
  return type == SubType || type == CmpType;
}

bool is_carry(LongLong value, Instruction::Size size) {
  switch (size) {
  case Instruction::ByteSize:
    return value & (value ^ 0xFF);
  case Instruction::WordSize:
    return value & (value ^ 0xFFFF);
  case Instruction::LongSize:
    return value & (value ^ 0xFFFFFFFF);
  default:
    std::unreachable();
  }
}

bool is_zero(LongLong value, Instruction::Size size) {
  switch (size) {
  case Instruction::ByteSize:
    return (value & 0xFF) == 0;
  case Instruction::WordSize:
    return (value & 0xFFFF) == 0;
  case Instruction::LongSize:
    return (value & 0xFFFFFFFF) == 0;
  default:
    std::unreachable();
  }
}

bool calculate_condition(const Registers& regs, Instruction::Condition cond) {
  switch (cond) {
  case Instruction::TrueCond:
    return true;
  case Instruction::FalseCond:
    return false;
  case Instruction::HigherCond:
    return (not regs.sr.carry) && (not regs.sr.zero);
  case Instruction::LowerOrSameCond:
    return regs.sr.carry || regs.sr.zero;
  case Instruction::CarryClearCond:
    return not regs.sr.carry;
  case Instruction::CarrySetCond:
    return regs.sr.carry;
  case Instruction::NotEqualCond:
    return not regs.sr.zero;
  case Instruction::EqualCond:
    return regs.sr.zero;
  case Instruction::OverflowClearCond:
    return not regs.sr.overflow;
  case Instruction::OverflowSetCond:
    return regs.sr.overflow;
  case Instruction::PlusCond:
    return not regs.sr.negative;
  case Instruction::MinusCond:
    return regs.sr.negative;
  case Instruction::GreaterOrEqualCond:
    return not(regs.sr.negative xor regs.sr.overflow);
  case Instruction::LessThanCond:
    return regs.sr.negative xor regs.sr.overflow;
  case Instruction::GreaterThanCond:
    return (regs.sr.negative and regs.sr.overflow and (not regs.sr.zero)) or
           ((not regs.sr.negative) and (not regs.sr.overflow) and (not regs.sr.zero));
  case Instruction::LessOrEqualCond:
    return regs.sr.zero or (regs.sr.negative and (not regs.sr.overflow)) or
           ((not regs.sr.negative) and regs.sr.overflow);
  default:
    std::unreachable();
  }
}

uint8_t bit_count(Instruction::Size size) {
  return size << 3;
}

bool msb(LongLong value, Instruction::Size size) {
  return (value >> (bit_count(size) - 1)) & 1;
}

bool is_overflow(LongLong lhs, LongLong rhs, LongLong result, Instruction::Size size, OpcodeType type = AddType) {
  const bool lhsMsb = msb(lhs, size) ^ (is_substract_op(type) ? 1 : 0);
  const bool rhsMsb = msb(rhs, size);
  const bool resultMsb = msb(result, size);
  return (lhsMsb && rhsMsb && !resultMsb) || (!lhsMsb && !rhsMsb && resultMsb);
}

} // namespace

std::optional<Error> Instruction::execute(Context ctx) {
#define SAFE_CALL(arg)                                                                                                 \
  if (auto err = (arg)) {                                                                                              \
    return std::move(err);                                                                                             \
  }

#define SAFE_DECLARE(name, init)                                                                                       \
  const auto name = init;                                                                                              \
  if (!(name)) {                                                                                                       \
    return (name).error();                                                                                             \
  }

  const auto push_stack = [&]<std::integral T>(T value) -> std::optional<Error> {
    auto& sp = ctx.registers.stack_ptr();
    auto& m = ctx.device;

    // reserve memory on the stack and save the value
    sp -= sizeof(value);
    SAFE_CALL(ctx.device.write(sp, value));
    return std::nullopt;
  };

  const auto pop_stack = [&]<std::integral T>(T& value) -> std::optional<Error> {
    auto& sp = ctx.registers.stack_ptr();
    auto& m = ctx.device;

    // dump the value and free memory on the stack
    SAFE_DECLARE(val, ctx.device.read<T>(sp));
    value = *val;
    sp += sizeof(T);
    return std::nullopt;
  };

  const auto displace_pc = [&] [[nodiscard]] (bool ignore_parsed_word_always = false) -> std::optional<Error> {
    auto& pc = ctx.registers.pc;
    if (size_ == ByteSize) {
      const auto offset = static_cast<SignedByte>(data_);
      pc += offset;
    } else {
      const auto offset = static_cast<SignedWord>(data_);
      pc += offset;

      // ignore the parsed word
      if (offset < 0 || ignore_parsed_word_always) {
        pc -= 2;
      }
    }
    if (pc & 1) {
      return Error{Error::UnalignedProgramCounter, fmt::format("program counter set at {:04x}", pc)};
    }
    return std::nullopt;
  };

  std::size_t inc_count = 1;

  const auto try_inc_address = [&](Target& target, bool has_flag, bool& used_flag) {
    if (has_flag && !used_flag) {
      target.try_increment_address(ctx, inc_count);
    }
    used_flag = true;
  };

  auto try_inc_address_src = [&, used_src_inc = false] mutable {
    return try_inc_address(src_, has_src_, used_src_inc);
  };

  auto try_inc_address_dst = [&, used_dst_inc = false] mutable {
    return try_inc_address(dst_, has_dst_, used_dst_inc);
  };

  if (has_src_) {
    src_.set_inc_or_dec_count(1);
  }
  if (has_dst_) {
    dst_.set_inc_or_dec_count(1);
  }

  switch (kind_) {
  case AbcdKind: {
    SAFE_DECLARE(src_val, src_.read<Byte>(ctx));
    SAFE_DECLARE(dst_val, dst_.read<Byte>(ctx));
    const Byte extend_flag = ctx.registers.sr.extend;

    const Word binary_result = *src_val + *dst_val + extend_flag;

    bool carry = false;
    int lval = (*src_val & 0x0F) + (*dst_val & 0x0F) + extend_flag;
    if (lval > 9) {
      carry = true;
      lval -= 10;
    }

    int hval = ((*src_val >> 4) & 0x0F) + ((*dst_val >> 4) & 0x0F) + (carry ? 1 : 0);
    carry = false;

    if (lval >= 16) {
      lval -= 16;
      ++hval;
    }

    if (hval > 9) {
      carry = true;
      hval -= 10;
    }

    const Word result = ((hval << 4) + lval) & 0xFF;

    SAFE_CALL(dst_.write<Byte>(ctx, result));
    ctx.registers.sr.negative = msb(result, ByteSize);
    ctx.registers.sr.carry = carry;
    ctx.registers.sr.extend = carry;
    ctx.registers.sr.overflow = (~binary_result & result & 0x80) != 0;
    if (result != 0) {
      ctx.registers.sr.zero = result == 0;
    }
    break;
  }
  case SbcdKind:
  case NbcdKind: {
    Byte byte0;
    Byte byte1;
    if (kind_ == SbcdKind) {
      SAFE_DECLARE(src_val, src_.read<Byte>(ctx));
      SAFE_DECLARE(dst_val, dst_.read<Byte>(ctx));
      byte0 = *dst_val;
      byte1 = *src_val;
    } else {
      SAFE_DECLARE(dst_val, dst_.read<Byte>(ctx));
      byte0 = 0;
      byte1 = *dst_val;
    }

    const Byte extend_flag = ctx.registers.sr.extend;
    const Word binary_result = byte0 - byte1 - extend_flag;

    bool carry = false;
    int lval = (byte0 & 0x0F) - (byte1 & 0x0F) - extend_flag;
    if (lval < 0) {
      carry = true;
      lval += 10;
    }

    int hval = ((byte0 >> 4) & 0x0F) - ((byte1 >> 4) & 0x0F) - (carry ? 1 : 0);
    carry = false;

    if (hval < 0) {
      carry = true;
      hval += 10;
    }

    if (hval == 0 && lval < 0) {
      carry = true;
    }

    const Word result = ((hval << 4) + lval) & 0xFF;

    SAFE_CALL(dst_.write<Byte>(ctx, result));
    ctx.registers.sr.negative = msb(result, ByteSize);
    ctx.registers.sr.carry = carry;
    ctx.registers.sr.extend = carry;
    ctx.registers.sr.overflow = (binary_result & ~result & 0x80) != 0;
    if (result != 0) {
      ctx.registers.sr.zero = result == 0;
    }
    break;
  }
  case AddKind:
  case AddiKind:
  case AndKind:
  case AndiKind:
  case CmpKind:
  case CmpiKind:
  case CmpmKind:
  case EorKind:
  case EoriKind:
  case OrKind:
  case OriKind:
  case SubKind:
  case SubiKind: {
    SAFE_DECLARE(src_val, src_.read_as_long_long(ctx, size_));
    try_inc_address_src();
    SAFE_DECLARE(dst_val, dst_.read_as_long_long(ctx, size_));

    const auto type = opcode_type(kind_);
    const LongLong result = do_binary_op(type, *src_val, *dst_val);
    if (type != CmpType) {
      SAFE_CALL(dst_.write_sized(ctx, result, size_));
    }

    const bool carry = is_carry(result, size_);
    if (type == AddType || type == SubType) {
      ctx.registers.sr.extend = carry;
    }
    ctx.registers.sr.negative = msb(result, size_);
    ctx.registers.sr.zero = is_zero(result, size_);
    if (type == AddType || type == SubType || type == CmpType) {
      ctx.registers.sr.overflow = is_overflow(*src_val, *dst_val, result, size_, type);
      ctx.registers.sr.carry = carry;
    } else {
      ctx.registers.sr.overflow = 0;
      ctx.registers.sr.carry = 0;
    }
    break;
  }
  case AddaKind:
  case CmpaKind:
  case SubaKind: {
    const auto type = opcode_type(kind_);

    LongLong src;
    if (size_ == WordSize) {
      SAFE_DECLARE(src_val, src_.read<Word>(ctx));
      src = static_cast<SignedLongLong>(static_cast<SignedWord>(*src_val));
    } else {
      SAFE_DECLARE(src_val, src_.read<Long>(ctx));
      src = *src_val;
    }
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
    const LongLong result = do_binary_op(type, src, *dst_val);

    if (type == CmpType) {
      const bool carry = is_carry(result ^ src, LongSize);
      ctx.registers.sr.negative = msb(result, LongSize);
      ctx.registers.sr.zero = is_zero(result, LongSize);
      ctx.registers.sr.overflow = is_overflow(src, *dst_val, result, LongSize, type);
      ctx.registers.sr.carry = carry;
    } else {
      SAFE_CALL(dst_.write_sized(ctx, result, LongSize));
    }
    break;
  }
  case AddqKind:
  case SubqKind: {
    const auto type = opcode_type(kind_);
    const LongLong src_val = data_ ? data_ : 8;
    SAFE_DECLARE(dst_val, dst_.read_as_long_long(ctx, size_));
    const LongLong result = do_binary_op(type, src_val, *dst_val);
    SAFE_CALL(dst_.write_sized(ctx, result, size_));

    if (dst_.kind() != Target::AddressRegisterKind) {
      const bool carry = is_carry(result, size_);
      ctx.registers.sr.negative = msb(result, size_);
      ctx.registers.sr.carry = carry;
      ctx.registers.sr.extend = carry;
      ctx.registers.sr.overflow = is_overflow(src_val, *dst_val, result, size_, type);
      ctx.registers.sr.zero = is_zero(result, size_);
    }
    break;
  }
  case AddxKind:
  case SubxKind: {
    const auto type = opcode_type(kind_);
    SAFE_DECLARE(src_val, src_.read_as_long_long(ctx, size_));
    SAFE_DECLARE(dst_val, dst_.read_as_long_long(ctx, size_));
    const LongLong result = do_binary_op(type, *src_val + ctx.registers.sr.extend, *dst_val);
    SAFE_CALL(dst_.write_sized(ctx, result, size_));

    const bool carry = is_carry(result, size_);
    ctx.registers.sr.negative = msb(result, size_);
    ctx.registers.sr.carry = carry;
    ctx.registers.sr.extend = carry;
    ctx.registers.sr.overflow = is_overflow(*src_val, *dst_val, result, size_, type);
    if (!is_zero(result, size_)) {
      ctx.registers.sr.zero = 0;
    }
    break;
  }
  case AndiToCcrKind:
  case EoriToCcrKind:
  case OriToCcrKind: {
    SAFE_DECLARE(src_val, src_.read<Byte>(ctx));
    auto& sr = ctx.registers.sr;
    sr = (sr & ~0xFF) | do_binary_op(opcode_type(kind_), sr & 0xFF, *src_val);
    break;
  }
  case MoveToCcrKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    auto& sr = ctx.registers.sr;
    sr = (sr & ~0xFF) | (*src_val & 0xFF);
    break;
  }
  case AndiToSrKind:
  case EoriToSrKind:
  case OriToSrKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    // TODO: find out why bits 12 and 14 matter
    ctx.registers.sr = do_binary_op(opcode_type(kind_), Word{ctx.registers.sr}, *src_val & 0b1010'1111'1111'1111);
    break;
  }
  case MoveToSrKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    try_inc_address_src();
    // TODO: find out why bits 12 and 14 matter
    ctx.registers.sr = *src_val & 0b1010'1111'1111'1111;
    break;
  }
  case MoveFromSrKind: {
    SAFE_CALL(dst_.write<Word>(ctx, ctx.registers.sr));
    break;
  }
  case MoveToUspKind: {
    SAFE_DECLARE(src_val, src_.read<Long>(ctx));
    ctx.registers.usp = *src_val;
    break;
  }
  case MoveFromUspKind: {
    SAFE_CALL(dst_.write<Long>(ctx, ctx.registers.usp));
    break;
  }
  case AslKind:
  case AsrKind:
  case LslKind:
  case LsrKind:
  case RolKind:
  case RorKind:
  case RoxlKind:
  case RoxrKind: {
    const bool is_arithmetic = kind_ == AslKind || kind_ == AsrKind;
    const bool is_rotate = kind_ == RolKind || kind_ == RorKind;
    const bool is_extend_rotate = kind_ == RoxlKind || kind_ == RoxrKind;
    const bool is_left = kind_ == AslKind || kind_ == LslKind || kind_ == RolKind || kind_ == RoxlKind;

    SAFE_DECLARE(dst_val, dst_.read_as_long_long(ctx, size_));

    uint8_t rotation;
    if (has_src_) {
      SAFE_DECLARE(src_val, src_.read_as_long_long(ctx, size_));
      rotation = *src_val % 64;
    } else {
      rotation = data_ ? data_ : 8;
    }

    LongLong result = *dst_val;
    bool has_overflow = false;
    bool cur_msb = msb(result, size_);
    bool last_bit_shifted;
    for (int i = 0; i < rotation; ++i) {
      if (is_left) {
        last_bit_shifted = msb(result, size_);
        result <<= 1;
        if (is_rotate) {
          result |= last_bit_shifted;
        } else if (is_extend_rotate) {
          result |= ctx.registers.sr.extend;
          ctx.registers.sr.extend = last_bit_shifted;
          ctx.registers.sr.carry = last_bit_shifted;
        }
      } else {
        if (i >= bit_count(size_) && is_arithmetic) {
          last_bit_shifted = 0;
        } else {
          last_bit_shifted = result & 1;
        }
        if (is_arithmetic) {
          // preserve the most significant bit
          result = (result >> 1) | (result & (1LL << (bit_count(size_) - 1)));
        } else {
          result >>= 1;
          if (is_rotate) {
            result |= static_cast<LongLong>(last_bit_shifted) << (bit_count(size_) - 1);
          }
          if (is_extend_rotate) {
            result |= static_cast<LongLong>(ctx.registers.sr.extend) << (bit_count(size_) - 1);
            ctx.registers.sr.extend = last_bit_shifted;
          }
        }
      }
      bool new_msb = msb(result, size_);
      if (cur_msb != new_msb) {
        has_overflow = true;
      }
      cur_msb = new_msb;
    }

    SAFE_CALL(dst_.write_sized(ctx, result, size_));

    ctx.registers.sr.negative = msb(result, size_);
    ctx.registers.sr.zero = is_zero(result, size_);
    if (is_arithmetic) {
      ctx.registers.sr.overflow = has_overflow;
    } else {
      ctx.registers.sr.overflow = 0;
    }
    if (rotation == 0) {
      ctx.registers.sr.carry = 0;
      if (is_extend_rotate) {
        ctx.registers.sr.carry = ctx.registers.sr.extend;
      }
    } else {
      if (!is_rotate && !is_extend_rotate) {
        ctx.registers.sr.extend = last_bit_shifted;
      }
      ctx.registers.sr.carry = last_bit_shifted;
    }
    break;
  }
  case BccKind: {
    if (calculate_condition(ctx.registers, cond_)) {
      SAFE_CALL(displace_pc(/*ignore_parsed_word_always=*/true));
    }
    break;
  }
  case DbccKind: {
    if (!calculate_condition(ctx.registers, cond_)) {
      SAFE_DECLARE(dst_val, dst_.read<Word>(ctx));
      auto counter = static_cast<SignedWord>(*dst_val);
      --counter;
      SAFE_CALL(dst_.write<Word>(ctx, counter));
      if (counter != -1) {
        // Dirty hack because BDcc has special displacement
        if (static_cast<SignedWord>(data_) >= 0) {
          ctx.registers.pc -= 2;
        }
        SAFE_CALL(displace_pc());
      }
    }
    break;
  }
  case SccKind: {
    if (calculate_condition(ctx.registers, cond_)) {
      SAFE_CALL(dst_.write<Byte>(ctx, 0xFF));
    } else {
      SAFE_CALL(dst_.write<Byte>(ctx, 0x00));
    }
    break;
  }
  case BsrKind: {
    push_stack(ctx.registers.pc);
    SAFE_CALL(displace_pc(/*ignore_parsed_word_always=*/true));
    break;
  }
  case JmpKind:
  case JsrKind: {
    Long old_pc = ctx.registers.pc;
    ctx.registers.pc = dst_.effective_address(ctx);
    if (kind_ == JsrKind) {
      push_stack(old_pc);
    }
    if (ctx.registers.pc & 1) {
      return Error{Error::UnalignedProgramCounter, fmt::format("program counter set at {:04x}", ctx.registers.pc)};
    }
    break;
  }
  case LeaKind: {
    SAFE_CALL(dst_.write<Long>(ctx, src_.effective_address(ctx)));
    break;
  }
  case PeaKind: {
    push_stack(src_.effective_address(ctx));
    break;
  }
  case BchgKind:
  case BclrKind:
  case BsetKind:
  case BtstKind: {
    // read bit number
    SAFE_DECLARE(src_val, src_.read<Byte>(ctx));
    auto bit_num = *src_val;
    if (dst_.kind() == Target::DataRegisterKind) {
      bit_num %= 32;
    } else {
      bit_num %= 8;
    }

    // read destination value
    LongLong val;
    if (dst_.kind() == Target::DataRegisterKind) {
      SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
      val = *dst_val;
    } else {
      SAFE_DECLARE(dst_val, dst_.read<Byte>(ctx));
      val = *dst_val;
    }

    const auto mask = 1LL << bit_num;
    auto newVal = val;
    if (kind_ == BchgKind) {
      newVal ^= mask;
    } else if (kind_ == BclrKind) {
      newVal &= newVal ^ mask;
    } else if (kind_ == BsetKind) {
      newVal |= mask;
    }

    // update Z flag and write value
    ctx.registers.sr.zero = not(val & mask);
    if (newVal != val) {
      if (dst_.kind() == Target::DataRegisterKind) {
        SAFE_CALL(dst_.write<Long>(ctx, newVal));
      } else {
        SAFE_CALL(dst_.write<Byte>(ctx, newVal));
      }
    }

    break;
  }
  case ClrKind:
  case NegKind:
  case NegxKind:
  case NotKind: {
    SAFE_DECLARE(dst_val, dst_.read_as_long_long(ctx, size_));
    auto result = *dst_val;

    bool has_overflow = false;

    if (kind_ == ClrKind) {
      result = 0;
    } else if (kind_ == NotKind) {
      result = ~result;
    } else if (kind_ == NegKind || kind_ == NegxKind) {
      result = ~result;

      if (kind_ != NegxKind || !ctx.registers.sr.extend) {
        const auto mask0 = (1LL << (bit_count(size_) - 1)) - 1;
        const auto mask1 = (1LL << bit_count(size_)) - 1;
        if ((result & mask1) == mask0) {
          has_overflow = true;
        }
        ++result;
      }
    }

    SAFE_CALL(dst_.write_sized(ctx, result, size_));

    ctx.registers.sr.negative = msb(result, size_);
    const bool cur_is_zero = is_zero(result, size_);
    if (kind_ != NegxKind || !cur_is_zero) {
      ctx.registers.sr.zero = cur_is_zero;
    }
    if (kind_ == NegKind || kind_ == NegxKind) {
      ctx.registers.sr.overflow = has_overflow;
      ctx.registers.sr.carry = is_carry(result, size_);
      ctx.registers.sr.extend = ctx.registers.sr.carry;
    } else {
      ctx.registers.sr.overflow = 0;
      ctx.registers.sr.carry = 0;
    }
    break;
  }
  case MoveKind: {
    auto tmp = ctx.registers.pc;
    ctx.registers.pc = data_;
    SAFE_DECLARE(src_val, src_.read_as_long_long(ctx, size_));
    try_inc_address_src();
    ctx.registers.pc = tmp;

    SAFE_CALL(dst_.write_sized(ctx, *src_val, size_));

    ctx.registers.sr.negative = msb(*src_val, size_);
    ctx.registers.sr.zero = is_zero(*src_val, size_);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case MovepKind: {
    if (dst_.kind() == Target::DataRegisterKind) {
      auto addr = src_.effective_address(ctx);
      const bool is_odd = addr & 1;
      if (is_odd) {
        --addr;
      }
      if (size_ == WordSize) {
        SAFE_DECLARE(word0, ctx.device.read<Word>(addr));
        SAFE_DECLARE(word1, ctx.device.read<Word>(addr + 2));
        Word result;
        if (is_odd) {
          result = ((*word0 & 0xFF) << 8) | (*word1 & 0xFF);
        } else {
          result = (*word0 & 0xFF00) | ((*word1 & 0xFF00) >> 8);
        }
        SAFE_CALL(dst_.write(ctx, result));
      } else if (size_ == LongSize) {
        SAFE_DECLARE(word0, ctx.device.read<Word>(addr));
        SAFE_DECLARE(word1, ctx.device.read<Word>(addr + 2));
        SAFE_DECLARE(word2, ctx.device.read<Word>(addr + 4));
        SAFE_DECLARE(word3, ctx.device.read<Word>(addr + 6));
        Long result;
        if (is_odd) {
          result = ((Long{*word0} & 0xFF) << 24) | ((Long{*word1} & 0xFF) << 16) | ((Long{*word2} & 0xFF) << 8) |
                   (Long{*word3} & 0xFF);
        } else {
          result = ((Long{*word0} & 0xFF00) << 16) | ((Long{*word1} & 0xFF00) << 8) | (Long{*word2} & 0xFF00) |
                   ((Long{*word3} & 0xFF00) >> 8);
        }
        SAFE_CALL(dst_.write(ctx, result));
      } else {
        std::unreachable();
      }
    } else {
      auto addr = dst_.effective_address(ctx);
      const bool is_odd = addr & 1;
      if (is_odd) {
        --addr;
      }
      if (size_ == WordSize) {
        SAFE_DECLARE(reg, src_.read<Word>(ctx));
        const Byte byte0 = (*reg & 0xFF00) >> 8;
        const Byte byte1 = (*reg & 0x00FF);
        if (is_odd) {
          SAFE_CALL(ctx.device.write<Word>(addr, byte0));
          SAFE_CALL(ctx.device.write<Word>(addr + 2, byte1));
        } else {
          SAFE_CALL(ctx.device.write<Word>(addr, byte0 << 8));
          SAFE_CALL(ctx.device.write<Word>(addr + 2, byte1 << 8));
        }
      } else if (size_ == LongSize) {
        SAFE_DECLARE(reg, src_.read<Long>(ctx));
        const Byte byte0 = (*reg & 0xFF000000) >> 24;
        const Byte byte1 = (*reg & 0x00FF0000) >> 16;
        const Byte byte2 = (*reg & 0x0000FF00) >> 8;
        const Byte byte3 = (*reg & 0x000000FF);
        if (is_odd) {
          SAFE_CALL(ctx.device.write<Word>(addr, byte0));
          SAFE_CALL(ctx.device.write<Word>(addr + 2, byte1));
          SAFE_CALL(ctx.device.write<Word>(addr + 4, byte2));
          SAFE_CALL(ctx.device.write<Word>(addr + 6, byte3));
        } else {
          SAFE_CALL(ctx.device.write<Word>(addr, byte0 << 8));
          SAFE_CALL(ctx.device.write<Word>(addr + 2, byte1 << 8));
          SAFE_CALL(ctx.device.write<Word>(addr + 4, byte2 << 8));
          SAFE_CALL(ctx.device.write<Word>(addr + 6, byte3 << 8));
        }
      } else {
        std::unreachable();
      }
    }
    break;
  }
  case MoveaKind: {
    auto tmp = ctx.registers.pc;
    ctx.registers.pc = data_;

    LongLong src;
    if (size_ == WordSize) {
      SAFE_DECLARE(src_val, src_.read<Word>(ctx));
      src = static_cast<SignedLongLong>(static_cast<SignedWord>(*src_val));
    } else {
      SAFE_DECLARE(src_val, src_.read<Long>(ctx));
      src = *src_val;
    }

    try_inc_address_src();
    ctx.registers.pc = tmp;

    SAFE_CALL(dst_.write<Long>(ctx, src));
    break;
  }
  case MovemKind: {
    const auto has_bit = [&](std::size_t i) { return data_ & (1 << i); };

    const auto get_reg = [&](std::size_t i) -> Long& {
      if (i <= 7) {
        return ctx.registers.d[i];
      } else if (i <= 14) {
        return ctx.registers.a[i - 8];
      } else {
        return ctx.registers.stack_ptr();
      }
    };

    if (has_src_) {
      const std::size_t reg_count = std::popcount(data_);
      inc_count = reg_count;
      std::array<Byte, 8 * sizeof(Word) * LongSize> data;
      assert(reg_count * size_ <= data.size());
      SAFE_CALL(src_.read(ctx, {data.data(), reg_count * size_}));

      std::size_t cur_pos = 0;
      for (std::size_t i = 0; i < 16; ++i) {
        if (has_bit(i)) {
          // a corner case: don't write to the postincrement register
          if (i < 8 || src_.kind() != Target::AddressIncrementKind || i - 8 != src_.index()) {
            if (size_ == WordSize) {
              SignedLong l = static_cast<SignedWord>((data[cur_pos] << 8) + data[cur_pos + 1]);
              get_reg(i) = l;
            } else {
              get_reg(i) =
                  (data[cur_pos] << 24) + (data[cur_pos + 1] << 16) + (data[cur_pos + 2] << 8) + data[cur_pos + 3];
            }
          }
          cur_pos += size_;
        }
      }
    } else {
      std::array<Byte, 64> data;
      size_t size = 0;
      for (std::size_t i = 0; i < 16; ++i) {
        bool has = has_bit(i);
        if (dst_.kind() == Target::AddressDecrementKind) {
          has = has_bit(15 - i);
        }

        if (has) {
          const Long reg = get_reg(i);
          if (size_ == LongSize) {
            data[size++] = (reg >> 24) & 0xFF;
            data[size++] = (reg >> 16) & 0xFF;
          }
          data[size++] = (reg >> 8) & 0xFF;
          data[size++] = reg & 0xFF;
        }
      }
      dst_.set_inc_or_dec_count(std::popcount(data_));
      SAFE_CALL(dst_.write(ctx, {data.data(), size}));
    }
    break;
  }
  case MoveqKind: {
    LongLong src = static_cast<SignedLongLong>(static_cast<SignedByte>(data_));
    SAFE_CALL(dst_.write<Long>(ctx, src));

    ctx.registers.sr.negative = msb(src, LongSize);
    ctx.registers.sr.zero = is_zero(src, LongSize);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case SwapKind: {
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
    Long val = *dst_val;
    Long half = val & 0xFFFF;
    val = (val >> 16) | (half << 16);
    SAFE_CALL(dst_.write<Long>(ctx, val));

    ctx.registers.sr.negative = msb(val, LongSize);
    ctx.registers.sr.zero = is_zero(val, LongSize);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case TasKind: {
    SAFE_DECLARE(dst_val, dst_.read<Byte>(ctx));
    Byte newVal = *dst_val | (1 << 7);
    SAFE_CALL(dst_.write<Byte>(ctx, newVal));

    ctx.registers.sr.negative = msb(*dst_val, ByteSize);
    ctx.registers.sr.zero = is_zero(*dst_val, ByteSize);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case ExgKind: {
    SAFE_DECLARE(src_val, src_.read<Long>(ctx));
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
    SAFE_CALL(dst_.write<Long>(ctx, *src_val));
    SAFE_CALL(src_.write<Long>(ctx, *dst_val));
    break;
  }
  case ExtKind: {
    Long val;
    if (size_ == WordSize) {
      SAFE_DECLARE(dst_val, dst_.read<Word>(ctx));
      const Word ext = *dst_val & (1 << 7) ? 0xFF : 0x00;
      val = (ext << 8) | (*dst_val & 0xFF);
      SAFE_CALL(dst_.write<Word>(ctx, val));
    } else /* Size_ == Long */ {
      SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
      const Long ext = *dst_val & (1 << 15) ? 0xFFFF : 0x0000;
      val = (ext << 16) | (*dst_val & 0xFFFF);
      SAFE_CALL(dst_.write<Long>(ctx, val));
    }
    ctx.registers.sr.negative = msb(val, size_);
    ctx.registers.sr.zero = is_zero(val, size_);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case LinkKind: {
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
    auto& sp = ctx.registers.stack_ptr();
    if (dst_.index() == 7) {
      // a really dirty hack to deal with "[LINK A7, #]"
      push_stack(*dst_val - 4);
    } else {
      push_stack(*dst_val);
    }

    SAFE_CALL(dst_.write<Long>(ctx, sp));
    auto offset = static_cast<SignedWord>(data_);
    sp += offset;
    break;
  }
  case UnlinkKind: {
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));
    ctx.registers.stack_ptr() = *dst_val;
    Long value;
    SAFE_CALL(pop_stack(value));
    SAFE_CALL(dst_.write<Long>(ctx, value));
    break;
  }
  case TrapKind:
  case TrapvKind: {
    if (kind_ == TrapvKind && !ctx.registers.sr.overflow) {
      break;
    }

    ctx.registers.sr.supervisor = 1;
    push_stack(ctx.registers.pc);
    push_stack(Word{ctx.registers.sr});

    SAFE_DECLARE(new_pc, ctx.device.read<Long>(data_ * 4));
    ctx.registers.pc = *new_pc;
    break;
  }
  case RteKind:
  case RtrKind:
  case RtsKind: {
    Word newSr;
    if (kind_ != RtsKind) {
      pop_stack(newSr);
    }
    pop_stack(ctx.registers.pc);

    if (kind_ == RteKind) {
      // TODO: find out why bits 12 and 14 matter
      ctx.registers.sr = newSr & 0b1010'1111'1111'1111;
    } else if (kind_ == RtrKind) {
      ctx.registers.sr = (ctx.registers.sr & 0xFF00) | (newSr & 0x00FF);
    }

    if (ctx.registers.pc & 1) {
      return Error{Error::UnalignedProgramCounter, fmt::format("program counter set at {:04x}", ctx.registers.pc)};
    }
    break;
  }
  case TstKind: {
    SAFE_DECLARE(src_val, src_.read_as_long_long(ctx, size_));
    ctx.registers.sr.negative = msb(*src_val, size_);
    ctx.registers.sr.zero = is_zero(*src_val, size_);
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.carry = 0;
    break;
  }
  case ChkKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    SAFE_DECLARE(dst_val, dst_.read<Word>(ctx));
    const auto signed_src = static_cast<SignedWord>(*src_val);
    const auto signed_dst = static_cast<SignedWord>(*dst_val);
    if (signed_dst < 0 || signed_dst > signed_src) {
      ctx.registers.sr.supervisor = 1;
      push_stack(ctx.registers.pc);
      push_stack(Word{ctx.registers.sr});

      constexpr int CHK_VECTOR = 6;
      SAFE_DECLARE(new_pc, ctx.device.read<Long>(CHK_VECTOR * 4));
      ctx.registers.pc = *new_pc;

      ctx.registers.sr.negative = signed_dst < 0;
    }
    auto& sr = ctx.registers.sr;
    sr.zero = sr.overflow = sr.carry = 0;
    break;
  }
  case MuluKind:
  case MulsKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    SAFE_DECLARE(dst_val, dst_.read<Word>(ctx));

    Long result;
    if (kind_ == MuluKind) {
      result = *src_val;
      result *= *dst_val;
    } else {
      SignedLong signed_result = static_cast<SignedWord>(*src_val);
      signed_result *= static_cast<SignedWord>(*dst_val);
      result = static_cast<Long>(signed_result);
    }

    SAFE_CALL(dst_.write(ctx, result));

    ctx.registers.sr.negative = msb(result, LongSize);
    ctx.registers.sr.carry = 0;
    ctx.registers.sr.overflow = 0;
    ctx.registers.sr.zero = result == 0;
    break;
  }
  case DivuKind:
  case DivsKind: {
    SAFE_DECLARE(src_val, src_.read<Word>(ctx));
    SAFE_DECLARE(dst_val, dst_.read<Long>(ctx));

    if (*src_val == 0) {
      ctx.registers.sr.supervisor = 1;
      push_stack(ctx.registers.pc);
      push_stack(Word{ctx.registers.sr});

      constexpr int DIVISION_BY_ZERO_VECTOR = 5;
      SAFE_DECLARE(new_pc, ctx.device.read<Long>(DIVISION_BY_ZERO_VECTOR * 4));
      ctx.registers.pc = *new_pc;

      auto& sr = ctx.registers.sr;
      sr.negative = sr.zero = sr.overflow = sr.carry = 0;
      break;
    }

    Long quotient;
    Long remainder;
    bool is_overflow = false;
    if (kind_ == DivuKind) {
      quotient = *dst_val / *src_val;
      remainder = *dst_val % *src_val;
      is_overflow = quotient > 0xFFFF;
    } else {
      const auto signed_dst_val = static_cast<SignedLong>(*dst_val);
      const auto signed_src_val = static_cast<SignedWord>(*src_val);
      const auto signed_quotient = signed_dst_val / signed_src_val;
      is_overflow = signed_quotient != static_cast<SignedWord>(signed_quotient);

      quotient = static_cast<Long>(signed_quotient);
      remainder = signed_dst_val % signed_src_val;
    }

    if (is_overflow) {
      ctx.registers.sr.overflow = 1;
    } else {
      Long result = remainder & 0xFFFF;
      result = (result << 16) | (quotient & 0xFFFF);
      SAFE_CALL(dst_.write(ctx, result));
      ctx.registers.sr.overflow = 0;
      ctx.registers.sr.negative = msb(quotient, WordSize);
      ctx.registers.sr.zero = quotient == 0;
    }
    ctx.registers.sr.carry = 0;
    break;
  }
  case NopKind:
  case ResetKind:
    break;
  }

  try_inc_address_src();
  try_inc_address_dst();

  return std::nullopt;
}

} // namespace m68k

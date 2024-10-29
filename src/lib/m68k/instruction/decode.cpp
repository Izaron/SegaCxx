#include "instruction.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fmt/core.h>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/common/context.h"
#include "lib/m68k/target/target.h"

namespace m68k {

namespace {

consteval Word calculate_mask(std::string_view pattern) {
  Word mask{};
  for (const char c : pattern) {
    if (c != ' ') {
      mask = (mask << 1) | ((c == '0' || c == '1') ? 1 : 0);
    }
  }
  return mask;
}

consteval Word calculate_value(std::string_view pattern) {
  Word mask{};
  for (const char c : pattern) {
    if (c != ' ') {
      mask = (mask << 1) | ((c == '1') ? 1 : 0);
    }
  }
  return mask;
}

} // namespace

std::expected<Instruction, Error> Instruction::decode(Context ctx) {
  const auto read_word = [&ctx] {
    auto& pc = ctx.registers.pc;
    const auto word = ctx.device.read<Word>(pc);
    if (word) {
      pc += 2;
    }
    return word;
  };

#define READ_WORD_SAFE                                                                                                 \
  const auto word = read_word();                                                                                       \
  if (!word) {                                                                                                         \
    return std::unexpected{word.error()};                                                                              \
  }

#define READ_TWO_WORDS_SAFE                                                                                            \
  const auto word0 = read_word();                                                                                      \
  if (!word0) {                                                                                                        \
    return std::unexpected{word0.error()};                                                                             \
  }                                                                                                                    \
  const auto word1 = read_word();                                                                                      \
  if (!word1) {                                                                                                        \
    return std::unexpected{word0.error()};                                                                             \
  }

#define HAS_PATTERN(pattern) ((*word & calculate_mask(pattern)) == calculate_value(pattern))

  // read the current word (16 bits)
  READ_WORD_SAFE;

  // helper functions
  const auto bits_range = [&word](std::size_t begin, std::size_t len) { return (*word >> begin) & ((1 << len) - 1); };
  const auto bit_at = [&bits_range](std::size_t bit) { return bits_range(bit, 1); };

  // 00 -> byte, 01 -> word, 02 -> long
  const auto get_size0 = [&bits_range]() {
    switch (bits_range(6, 2)) {
    case 0:
      return ByteSize;
    case 1:
      return WordSize;
    case 2:
      return LongSize;
    default:
      std::unreachable();
    }
  };

  const auto parse_target_with_size = [&](Size size, std::size_t modeBegin,
                                          std::size_t indexBegin) -> std::expected<Target, Error> {
    Target target;

    const auto mode = bits_range(modeBegin, 3);
    const auto xn = bits_range(indexBegin, 3);

    switch (mode) {
    case 0:
      target.kind(Target::DataRegisterKind).index(xn);
      break;
    case 1:
      target.kind(Target::AddressRegisterKind).index(xn);
      break;
    case 2:
      target.kind(Target::AddressKind).index(xn);
      break;
    case 3:
      target.kind(Target::AddressIncrementKind).index(xn).size(size);
      break;
    case 4:
      target.kind(Target::AddressDecrementKind).index(xn).size(size);
      break;
    case 5: {
      READ_WORD_SAFE;
      target.kind(Target::AddressDisplacementKind).index(xn).ext_word0(*word);
      break;
    }
    case 6: {
      READ_WORD_SAFE;
      target.kind(Target::AddressIndexKind).index(xn).ext_word0(*word);
      break;
    }
    case 7: {
      switch (xn) {
      case 0: {
        READ_WORD_SAFE;
        target.kind(Target::AbsoluteShortKind).ext_word0(*word);
        break;
      }
      case 1: {
        READ_TWO_WORDS_SAFE;
        target.kind(Target::AbsoluteLongKind).ext_word0(*word0).ext_word1(*word1);
        break;
      }
      case 2: {
        READ_WORD_SAFE;
        target.kind(Target::ProgramCounterDisplacementKind).ext_word0(*word);
        break;
      }
      case 3: {
        READ_WORD_SAFE;
        target.kind(Target::ProgramCounterIndexKind).ext_word0(*word);
        break;
      }
      case 4: {
        auto& pc = ctx.registers.pc;
        target.kind(Target::ImmediateKind).address((size == ByteSize) ? (pc + 1) : pc);
        pc += (size == LongSize) ? 4 : 2;
        break;
      }
      default:
        return std::unexpected<Error>(
            {Error::UnknownAddressingMode, fmt::format("Unknown addresing mode in word {:04x}", *word)});
      }
      break;
    }
    default:
      std::unreachable();
    }

    return target;
  };

#define PARSE_TARGET_WITH_SIZE_SAFE(size)                                                                              \
  auto dst = parse_target_with_size(size, 3, 0);                                                                       \
  if (!dst) {                                                                                                          \
    return std::unexpected{dst.error()};                                                                               \
  }

#define PARSE_TARGET_WITH_ARGS_SAFE(dst, size, modeBegin, indexBegin)                                                  \
  auto(dst) = parse_target_with_size(size, modeBegin, indexBegin);                                                     \
  if (!(dst)) {                                                                                                        \
    return std::unexpected{(dst).error()};                                                                             \
  }

#define PARSE_TARGET_SAFE PARSE_TARGET_WITH_SIZE_SAFE(get_size0())

  // decode the opcode
  Instruction inst;

  /*
   * Status register instruction: [ANDI|EORI]to[CCR|SR]
   */
  const auto try_parse_status_register_instruction = [&]() -> std::expected<bool, Error> {
    using TCase = std::tuple<Kind, Kind, int>;
    constexpr std::array<TCase, 3> cases{
        std::make_tuple(OriToCcrKind, OriToSrKind, 0),
        std::make_tuple(AndiToCcrKind, AndiToSrKind, 1),
        std::make_tuple(EoriToCcrKind, EoriToSrKind, 5),
    };
    for (auto [ccr_kind, sr_kind, index] : cases) {
      if (HAS_PATTERN("0000 ...0 0.11 1100") && bits_range(9, 3) == index) {
        bool is_word = bit_at(6);

        auto& pc = ctx.registers.pc;
        auto src = Target{}.kind(Target::ImmediateKind).address(pc + (is_word ? 0 : 1));
        pc += 2;

        inst.kind(is_word ? sr_kind : ccr_kind).src(src);
        return true;
      }
    }
    return false;
  };

  /*
   * Bit manipulation instruction: BTST, BCHG, BCLR, BSET
   */
  const auto try_parse_bit_instruction = [&]() -> std::expected<bool, Error> {
#define CASE(case_kind, register_pattern, immediate_pattern)                                                           \
  if (HAS_PATTERN(register_pattern) && bits_range(3, 3) != 1) {                                                        \
    auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));                                        \
    PARSE_TARGET_WITH_SIZE_SAFE(ByteSize);                                                                             \
    inst.kind(case_kind).src(src).dst(*dst).size(ByteSize);                                                            \
    return true;                                                                                                       \
  }                                                                                                                    \
  if (HAS_PATTERN(immediate_pattern)) {                                                                                \
    auto& pc = ctx.registers.pc;                                                                                       \
    auto src = Target{}.kind(Target::ImmediateKind).address(pc + 1);                                                   \
    pc += 2;                                                                                                           \
    PARSE_TARGET_WITH_SIZE_SAFE(ByteSize);                                                                             \
    inst.kind(case_kind).src(src).dst(*dst).size(ByteSize);                                                            \
    return true;                                                                                                       \
  }
    CASE(BtstKind, "0000 ...1 00.. ....", "0000 1000 00.. ....")
    CASE(BchgKind, "0000 ...1 01.. ....", "0000 1000 01.. ....")
    CASE(BclrKind, "0000 ...1 10.. ....", "0000 1000 10.. ....")
    CASE(BsetKind, "0000 ...1 11.. ....", "0000 1000 11.. ....")
#undef CASE
    return false;
  };

  /*
   * Unary operations: NEG, NEGX, CLR, NOT
   */
  const auto try_parse_unary_instruction = [&]() -> std::expected<bool, Error> {
#define CASE(case_kind, case_pattern)                                                                                  \
  if (HAS_PATTERN(case_pattern) && bits_range(6, 2) != 3) {                                                            \
    PARSE_TARGET_SAFE;                                                                                                 \
    inst.kind(case_kind).dst(*dst).size(get_size0());                                                                  \
    return true;                                                                                                       \
  }
    CASE(NegxKind, "0100 0000 .... ....")
    CASE(ClrKind, "0100 0010 .... ....")
    CASE(NegKind, "0100 0100 .... ....")
    CASE(NotKind, "0100 0110 .... ....")
#undef CASE
    return false;
  };

  /*
   * Bit shift operations: ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR
   */
  const auto try_parse_shift_instruction = [&]() -> std::expected<bool, Error> {
    using TCase = std::tuple<Kind, Kind, int>;
    constexpr std::array<TCase, 4> cases{
        std::make_tuple(AslKind, AsrKind, 0),
        std::make_tuple(LslKind, LsrKind, 1),
        std::make_tuple(RoxlKind, RoxrKind, 2),
        std::make_tuple(RolKind, RorKind, 3),
    };

    for (auto [left_kind, right_kind, index] : cases) {
      if (HAS_PATTERN("1110 0... 11.. ....") && bits_range(9, 2) == index) {
        // operation on any memory, shift by 1
        auto kind = bit_at(8) ? left_kind : right_kind;
        PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
        inst.kind(kind).dst(*dst).size(WordSize).data(1);
        return true;
      }
      if (HAS_PATTERN("1110 .... .... ....") && bits_range(3, 2) == index && bits_range(6, 2) != 3) {
        // operation on Dn
        auto kind = bit_at(8) ? left_kind : right_kind;
        uint8_t rotation = bits_range(9, 3);
        auto dst = Target{}.kind(Target::DataRegisterKind).index(bits_range(0, 3));

        inst.kind(kind).dst(dst).size(get_size0());
        if (bit_at(5)) {
          // shift count is in the data register
          auto src = Target{}.kind(Target::DataRegisterKind).index(rotation);
          inst.src(src);
        } else {
          // shift count is immediate
          inst.data(rotation);
        }
        return true;
      }
    }
    return false;
  };

  /*
   * Binary operations on immediate: ADDI, ANDI, EORI, ORI
   */
  const auto try_parse_binary_on_immediate_instruction = [&]() -> std::expected<bool, Error> {
    using TCase = std::tuple<Kind, int>;
    constexpr std::array<TCase, 6> cases{
        std::make_tuple(OriKind, 0),  std::make_tuple(AndiKind, 1), std::make_tuple(SubiKind, 2),
        std::make_tuple(AddiKind, 3), std::make_tuple(EoriKind, 5), std::make_tuple(CmpiKind, 6),
    };

    for (auto [kind, index] : cases) {
      if (HAS_PATTERN("0000 ...0 .... ....") && bits_range(9, 3) == index) {
        auto& pc = ctx.registers.pc;
        auto src = Target{}.kind(Target::ImmediateKind).address((get_size0() == ByteSize) ? (pc + 1) : pc);
        pc += (get_size0() == LongSize) ? 4 : 2;

        PARSE_TARGET_SAFE;
        inst.kind(kind).src(src).dst(*dst).size(get_size0());
        return true;
      }
    }
    return false;
  };

  /*
   * Binary operations: ADD, AND, EOR, OR, SUB
   */
  const auto try_parse_binary_instruction = [&]() -> std::expected<bool, Error> {
    using TCase = std::tuple<Kind, int>;
    constexpr std::array<TCase, 5> cases{
        std::make_tuple(OrKind, 0),  std::make_tuple(SubKind, 1), std::make_tuple(EorKind, 3),
        std::make_tuple(AndKind, 4), std::make_tuple(AddKind, 5),
    };

    for (auto [kind, index] : cases) {
      if (HAS_PATTERN("1... .... .... ....") && bits_range(12, 3) == index) {
        auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));
        PARSE_TARGET_SAFE;
        if (!bit_at(8)) {
          if (kind == EorKind) {
            // some hack
            kind = CmpKind;
          }
          std::swap(src, *dst);
        }
        inst.kind(kind).src(src).dst(*dst).size(get_size0());
        return true;
      }
    }
    return false;
  };

  /*
   * Binary operations on address: ADDA, SUBA
   */
  const auto try_parse_binary_on_address_instruction = [&]() -> std::expected<bool, Error> {
    using TCase = std::tuple<Kind, int>;
    constexpr std::array<TCase, 3> cases{
        std::make_tuple(SubaKind, 0),
        std::make_tuple(CmpaKind, 1),
        std::make_tuple(AddaKind, 2),
    };

    for (auto [kind, index] : cases) {
      if (HAS_PATTERN("1..1 .... 11.. ....") && bits_range(13, 2) == index) {
        const auto size = bit_at(8) ? LongSize : WordSize;
        auto src = Target{}.kind(Target::AddressRegisterKind).index(bits_range(9, 3));
        PARSE_TARGET_WITH_SIZE_SAFE(size);
        std::swap(src, *dst);
        inst.kind(kind).src(src).dst(*dst).size(size);
        return true;
      }
    }
    return false;
  };

  /*
   * Moves: MOVE, MOVEM, MOVEA, MOVEP, MOVEQ, MOVEtoCCR, MOVE[to|from][SR|USP]
   */
  const auto try_parse_move_instruction = [&]() -> std::expected<bool, Error> {
    // MOVE/MOVEA
    if (HAS_PATTERN("00.. .... .... ....")) {
      std::optional<Size> size;
      switch (bits_range(12, 2)) {
      case 0b01:
        size = ByteSize;
        break;
      case 0b11:
        size = WordSize;
        break;
      case 0b10:
        size = LongSize;
        break;
      default:
        break;
      }
      if (size) {
        PARSE_TARGET_WITH_ARGS_SAFE(src, *size, 3, 0);
        Long pc = ctx.registers.pc; // remember current program counter
        PARSE_TARGET_WITH_ARGS_SAFE(dst, *size, 6, 9);
        const auto kind = bits_range(6, 3) == 1 ? MoveaKind : MoveKind;
        inst.kind(kind).src(*src).dst(*dst).size(*size).data(pc);
        return true;
      }
    }
    // MOVEP
    if (HAS_PATTERN("0000 ...1 ..00 1...")) {
      const auto size = bit_at(6) ? LongSize : WordSize;

      auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));

      READ_WORD_SAFE;
      auto dst = Target{}.kind(Target::AddressDisplacementKind).index(bits_range(0, 3)).ext_word0(*word);

      if (!bit_at(7)) {
        std::swap(src, dst);
      }
      inst.kind(MovepKind).src(src).dst(dst).size(size);
      return true;
    }
    // MOVEM
    if (HAS_PATTERN("0100 1.00 1... ....")) {
      READ_WORD_SAFE;
      const auto size = bit_at(6) ? LongSize : WordSize;
      PARSE_TARGET_WITH_SIZE_SAFE(size);
      inst.kind(MovemKind).data(*word).size(size);
      if (bit_at(10)) {
        inst.src(*dst);
      } else {
        inst.dst(*dst);
      }
      return true;
    }
    // MOVEQ
    if (HAS_PATTERN("0111 ...0 .... ....")) {
      auto dst = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));
      inst.kind(MoveqKind).data(bits_range(0, 8)).dst(dst);
      return true;
    }
    // MOVEtoCCR/MOVEtoSR
    if (HAS_PATTERN("0100 01.0 11.. ....")) {
      PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
      inst.kind(bit_at(9) ? MoveToSrKind : MoveToCcrKind).src(*dst);
      return true;
    }
    // MOVEfromSR
    if (HAS_PATTERN("0100 0000 11.. ....")) {
      PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
      inst.kind(MoveFromSrKind).dst(*dst);
      return true;
    }
    // MOVEtoUSP
    if (HAS_PATTERN("0100 1110 0110 0...")) {
      auto src = Target{}.kind(Target::AddressRegisterKind).index(bits_range(0, 3));
      inst.kind(MoveToUspKind).src(src);
      return true;
    }
    // MOVEfromUSP
    if (HAS_PATTERN("0100 1110 0110 1...")) {
      auto dst = Target{}.kind(Target::AddressRegisterKind).index(bits_range(0, 3));
      inst.kind(MoveFromUspKind).dst(dst);
      return true;
    }
    return false;
  };

  if (HAS_PATTERN("0100 1110 0111 0000")) {
    inst.kind(ResetKind);
  } else if (HAS_PATTERN("0100 1110 0111 0001")) {
    inst.kind(NopKind);
  } else if (HAS_PATTERN("0101 .... 1100 1...")) {
    const auto cond = static_cast<Condition>(bits_range(8, 4));
    auto dst = Target{}.kind(Target::DataRegisterKind).index(bits_range(0, 3)).size(WordSize);
    READ_WORD_SAFE;
    inst.kind(DbccKind).condition(cond).dst(dst).data(*word).size(WordSize);
  } else if (HAS_PATTERN("0101 .... 11.. ....")) {
    const auto cond = static_cast<Condition>(bits_range(8, 4));
    PARSE_TARGET_WITH_SIZE_SAFE(ByteSize);
    inst.kind(SccKind).condition(cond).dst(*dst);
  } else if (HAS_PATTERN("0101 .... .... ....")) {
    PARSE_TARGET_SAFE;
    inst.kind(bit_at(8) ? SubqKind : AddqKind).data(bits_range(9, 3)).dst(*dst).size(get_size0());
  } else if (HAS_PATTERN("1.00 ...1 0000 ....")) {
    const auto kind = bit_at(3) ? Target::AddressDecrementKind : Target::DataRegisterKind;
    auto src = Target{}.kind(kind).index(bits_range(0, 3)).size(1);
    auto dst = Target{}.kind(kind).index(bits_range(9, 3)).size(1);
    inst.kind(bit_at(14) ? AbcdKind : SbcdKind).src(src).dst(dst);
  } else if (HAS_PATTERN("1.01 ...1 ..00 ....") && bits_range(6, 2) != 3) {
    const auto size = get_size0();
    const auto kind = bit_at(3) ? Target::AddressDecrementKind : Target::DataRegisterKind;
    auto src = Target{}.kind(kind).index(bits_range(0, 3)).size(size);
    auto dst = Target{}.kind(kind).index(bits_range(9, 3)).size(size);
    inst.kind(bit_at(14) ? AddxKind : SubxKind).src(src).dst(dst).size(size);
  } else if (HAS_PATTERN("0110 .... .... ....")) {
    const auto cond = static_cast<Condition>(bits_range(8, 4));

    auto displacement = bits_range(0, 8);
    auto size = ByteSize;
    if (displacement == 0) {
      READ_WORD_SAFE;
      displacement = *word;
      size = WordSize;
    }

    // the False condition is actually a BSR (Branch to Subroutine)
    if (cond == FalseCond) {
      inst.kind(BsrKind).data(displacement).size(size);
    } else {
      inst.kind(BccKind).condition(cond).data(displacement).size(size);
    }
  } else if (HAS_PATTERN("0100 1110 1... ....")) {
    PARSE_TARGET_WITH_SIZE_SAFE(LongSize);
    const auto kind = bit_at(6) ? JmpKind : JsrKind;
    inst.kind(kind).dst(*dst);
  } else if (HAS_PATTERN("0100 ...1 11.. ....")) {
    PARSE_TARGET_WITH_SIZE_SAFE(LongSize);
    auto src = Target{}.kind(Target::AddressRegisterKind).index(bits_range(9, 3));
    std::swap(src, *dst);
    inst.kind(LeaKind).src(src).dst(*dst);
  } else if (HAS_PATTERN("1011 ...1 ..00 1...") && bits_range(6, 2) != 3) {
    const auto size = get_size0();
    auto src = Target{}.kind(Target::AddressIncrementKind).index(bits_range(0, 3)).size(size);
    auto dst = Target{}.kind(Target::AddressIncrementKind).index(bits_range(9, 3)).size(size);
    inst.kind(CmpmKind).src(src).dst(dst).size(size);
  } else if (HAS_PATTERN("0100 1000 0100 0...")) {
    auto dst = Target{}.kind(Target::DataRegisterKind).index(bits_range(0, 3));
    inst.kind(SwapKind).dst(dst);
  } else if (HAS_PATTERN("0100 1000 01.. ....")) {
    PARSE_TARGET_WITH_SIZE_SAFE(LongSize);
    inst.kind(PeaKind).src(*dst);
  } else if (HAS_PATTERN("0100 1010 11.. ....")) {
    PARSE_TARGET_WITH_SIZE_SAFE(ByteSize);
    inst.kind(TasKind).dst(*dst);
  } else if (HAS_PATTERN("1100 ...1 ..00 ....") && bits_range(6, 2) != 3) {
    auto dst = Target{}.index(bits_range(0, 3));
    auto src = Target{}.index(bits_range(9, 3));
    if (bits_range(3, 5) == 0b01000) {
      dst.kind(Target::DataRegisterKind);
      src.kind(Target::DataRegisterKind);
    } else if (bits_range(3, 5) == 0b01001) {
      dst.kind(Target::AddressRegisterKind);
      src.kind(Target::AddressRegisterKind);
    } else {
      dst.kind(Target::AddressRegisterKind);
      src.kind(Target::DataRegisterKind);
    }
    inst.kind(ExgKind).src(src).dst(dst);
  } else if (HAS_PATTERN("0100 1000 1.00 0...")) {
    auto dst = Target{}.kind(Target::DataRegisterKind).index(bits_range(0, 3));
    inst.kind(ExtKind).dst(dst).size(bit_at(6) ? LongSize : WordSize);
  } else if (HAS_PATTERN("0100 1110 0101 0...")) {
    auto dst = Target{}.kind(Target::AddressRegisterKind).index(bits_range(0, 3));
    READ_WORD_SAFE;
    inst.kind(LinkKind).dst(dst).data(*word);
  } else if (HAS_PATTERN("0100 1110 0101 1...")) {
    auto dst = Target{}.kind(Target::AddressRegisterKind).index(bits_range(0, 3));
    inst.kind(UnlinkKind).dst(dst);
  } else if (HAS_PATTERN("0100 1110 0100 ....")) {
    constexpr int TRAP_VECTOR_OFFSET = 32;
    inst.kind(TrapKind).data(TRAP_VECTOR_OFFSET + bits_range(0, 4));
  } else if (HAS_PATTERN("0100 1110 0111 0110")) {
    constexpr int TRAPV_VECTOR = 7;
    inst.kind(TrapvKind).data(TRAPV_VECTOR);
  } else if (HAS_PATTERN("0100 1110 0111 0011")) {
    inst.kind(RteKind);
  } else if (HAS_PATTERN("0100 1110 0111 0101")) {
    inst.kind(RtsKind);
  } else if (HAS_PATTERN("0100 1110 0111 0111")) {
    inst.kind(RtrKind);
  } else if (HAS_PATTERN("0100 1010 .... ....")) {
    PARSE_TARGET_SAFE;
    inst.kind(TstKind).src(*dst).size(get_size0());
  } else if (HAS_PATTERN("0100 ...1 10.. ....")) {
    auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));
    PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
    // note - swap `src` and `dst`
    inst.kind(ChkKind).src(*dst).dst(src).size(WordSize);
  } else if (HAS_PATTERN("0100 1000 00.. ....")) {
    PARSE_TARGET_WITH_SIZE_SAFE(ByteSize);
    inst.kind(NbcdKind).dst(*dst).size(ByteSize);
  } else if (HAS_PATTERN("1100 .... 11.. ....")) {
    auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));
    PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
    // note - swap `src` and `dst`
    inst.kind(bit_at(8) ? MulsKind : MuluKind).src(*dst).dst(src);
  } else if (HAS_PATTERN("1000 .... 11.. ....")) {
    auto src = Target{}.kind(Target::DataRegisterKind).index(bits_range(9, 3));
    PARSE_TARGET_WITH_SIZE_SAFE(WordSize);
    // note - swap `src` and `dst`
    inst.kind(bit_at(8) ? DivsKind : DivuKind).src(*dst).dst(src);
  } else {

#define TRY_PARSE_SAFE(func)                                                                                           \
  {                                                                                                                    \
    auto res = func();                                                                                                 \
    if (!res) {                                                                                                        \
      return std::unexpected{res.error()};                                                                             \
    }                                                                                                                  \
    if (*res) {                                                                                                        \
      return inst;                                                                                                     \
    }                                                                                                                  \
  }

    TRY_PARSE_SAFE(try_parse_status_register_instruction);
    TRY_PARSE_SAFE(try_parse_bit_instruction);
    TRY_PARSE_SAFE(try_parse_unary_instruction);
    TRY_PARSE_SAFE(try_parse_shift_instruction);
    TRY_PARSE_SAFE(try_parse_binary_on_address_instruction);
    TRY_PARSE_SAFE(try_parse_binary_on_immediate_instruction);
    TRY_PARSE_SAFE(try_parse_binary_instruction);
    TRY_PARSE_SAFE(try_parse_move_instruction);

    return std::unexpected<Error>{{Error::UnknownOpcode, fmt::format("Unknown opcode {:04x}", *word)}};
  }

  return inst;
}

} // namespace m68k

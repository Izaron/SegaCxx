#include "instruction.h"

#include <fmt/core.h>
#include <string>
#include <string_view>

#include "lib/common/util/unreachable.h"
#include "lib/m68k/target/target.h"

namespace m68k {

std::string Instruction::print() const {
  const auto format_size = [this] -> char {
    switch (size_) {
    case ByteSize:
      return 'b';
    case WordSize:
      return 'w';
    case LongSize:
      return 'l';
    default:
      unreachable();
    }
  };

#define NAME(kind, value)                                                                                              \
  if (kind_ == (kind)) {                                                                                               \
    name = value;                                                                                                      \
  }

  switch (kind_) {
  case AbcdKind:
  case SbcdKind: {
    std::string_view name;
    NAME(AbcdKind, "ABCD");
    NAME(SbcdKind, "SBCD");
    return fmt::format("{} {}, {}", name, src_.print(), dst_.print());
  }
  case OrKind:
  case OriKind:
  case AndKind:
  case AndiKind:
  case SubKind:
  case SubiKind:
  case AddKind:
  case AddiKind:
  case EorKind:
  case EoriKind:
  case CmpKind:
  case CmpiKind: {
  case CmpmKind:
    std::string_view name;
    NAME(OrKind, "OR");
    NAME(OriKind, "OR");
    NAME(AndKind, "AND");
    NAME(AndiKind, "AND");
    NAME(SubKind, "SUB");
    NAME(SubiKind, "SUB");
    NAME(AddKind, "ADD");
    NAME(AddiKind, "ADD");
    NAME(EorKind, "EOR");
    NAME(EoriKind, "EOR");
    NAME(CmpKind, "CMP");
    NAME(CmpiKind, "CMP");
    NAME(CmpmKind, "CMP");
    return fmt::format("{}.{} {}, {}", name, format_size(), src_.print(), dst_.print());
  }
  case SubaKind:
  case CmpaKind:
  case AddaKind: {
    std::string_view name;
    NAME(SubaKind, "SUBA");
    NAME(CmpaKind, "CMPA");
    NAME(AddaKind, "ADDA");
    return fmt::format("{}.{} {}, {}", name, format_size(), src_.print(), dst_.print());
  }
  case SubqKind:
  case AddqKind: {
    std::string_view name;
    NAME(SubqKind, "SUB");
    NAME(AddqKind, "ADD");
    return fmt::format("{}.{} Q, {}", name, format_size(), dst_.print());
  }
  case SubxKind:
  case AddxKind: {
    std::string_view name;
    NAME(SubxKind, "SUBX");
    NAME(AddxKind, "ADDX");
    return fmt::format("{}.{} {}, {}", name, format_size(), src_.print(), dst_.print());
  }
  case OriToCcrKind:
  case OriToSrKind:
  case AndiToCcrKind:
  case AndiToSrKind:
  case EoriToCcrKind:
  case EoriToSrKind: {
    std::string_view name;
    NAME(OriToCcrKind, "ORItoCCR");
    NAME(OriToSrKind, "ORItoSR");
    NAME(AndiToCcrKind, "ANDItoCCR");
    NAME(AndiToSrKind, "ANDItoSR");
    NAME(EoriToCcrKind, "EORItoCCR");
    NAME(EoriToSrKind, "EORItoSR");
    return fmt::format("{} {}", name, src_.print());
  }
  case AslKind:
  case AsrKind:
  case LslKind:
  case LsrKind:
  case RoxlKind:
  case RoxrKind:
  case RolKind:
  case RorKind: {
    std::string_view name;
    NAME(AslKind, "ASL");
    NAME(AsrKind, "ASR");
    NAME(LslKind, "LSL");
    NAME(LsrKind, "LSR");
    NAME(RoxlKind, "ROXL");
    NAME(RoxrKind, "ROXR");
    NAME(RolKind, "ROL");
    NAME(RorKind, "ROR");
    if (dst_.kind() == Target::DataRegisterKind) {
      return fmt::format("{}.{} {}, {}", name, format_size(), (has_src_ ? src_.print() : "Q"), dst_.print());
    } else {
      return fmt::format("{}.{} {}", name, format_size(), dst_.print());
    }
  }
  case BsrKind:
  case BccKind: {
    std::string_view name;
    NAME(BsrKind, "BSR");
    NAME(BccKind, "Bcc");
    return fmt::format("{} {}", name, (size_ == ByteSize) ? "Q" : "#");
  }
  case DbccKind:
    return fmt::format("DBcc {}, #", dst_.print());
  case BtstKind:
  case BchgKind:
  case BclrKind:
  case BsetKind: {
    std::string_view name;
    NAME(BtstKind, "BTST");
    NAME(BchgKind, "BCHG");
    NAME(BclrKind, "BCLR");
    NAME(BsetKind, "BSET");
    return fmt::format("{} {}, {}", name, src_.print(), dst_.print());
  }
  case ChkKind:
    return fmt::format("CHK {}, {}", src_.print(), dst_.print());
  case NegxKind:
  case ClrKind:
  case NegKind:
  case NotKind: {
    std::string_view name;
    NAME(NegxKind, "NEGX");
    NAME(ClrKind, "CLR");
    NAME(NegKind, "NEG");
    NAME(NotKind, "NOT");
    return fmt::format("{}.{} {}", name, format_size(), dst_.print());
  }
  case DivsKind:
  case DivuKind:
  case MulsKind:
  case MuluKind: {
    std::string_view name;
    NAME(DivsKind, "DIVS");
    NAME(DivuKind, "DIVU");
    NAME(MulsKind, "MULS");
    NAME(MuluKind, "MULU");
    return fmt::format("{} {}, {}", name, src_.print(), dst_.print());
  }
  case ExgKind:
    return fmt::format("EXG {}, {}", src_.print(), dst_.print());
  case ExtKind:
    return fmt::format("EXT.{} {}", format_size(), dst_.print());
  case JmpKind:
  case JsrKind: {
    std::string_view name;
    NAME(JmpKind, "JMP");
    NAME(JsrKind, "JSR");
    return fmt::format("{} {}", name, dst_.print());
  }
  case LeaKind:
    return fmt::format("LEA {}, {}", src_.print(), dst_.print());
  case LinkKind:
    return fmt::format("LINK {}, #", dst_.print());
  case MoveFromSrKind:
    return fmt::format("MOVEfromSR {}", dst_.print());
  case MoveFromUspKind:
    return fmt::format("MOVEfromUSP {}", dst_.print());
  case MoveKind:
  case MoveaKind: {
    std::string_view name;
    NAME(MoveKind, "MOVE");
    NAME(MoveaKind, "MOVEA");
    return fmt::format("{}.{} {}, {}", name, format_size(), src_.print(), dst_.print());
  }
  case MoveToCcrKind:
  case MoveToSrKind:
  case MoveToUspKind: {
    std::string_view name;
    NAME(MoveToCcrKind, "MOVEtoCCR");
    NAME(MoveToSrKind, "MOVEtoSR");
    NAME(MoveToUspKind, "MOVEtoUSP");
    return fmt::format("{} {}", name, src_.print());
  }
  case MovemKind:
    if (has_src_) {
      return fmt::format("MOVEM.{} {}, #", format_size(), src_.print());
    } else {
      return fmt::format("MOVEM.{} #, {}", format_size(), dst_.print());
    }
  case MovepKind:
    return fmt::format("MOVEP.{} {}, {}", format_size(), src_.print(), dst_.print());
  case MoveqKind:
    return fmt::format("MOVE.q Q, {}", dst_.print());
  case NbcdKind:
    return fmt::format("NBCD {}", dst_.print());
  case SccKind:
    return fmt::format("Scc {}", dst_.print());
  case SwapKind:
    return fmt::format("SWAP {}", dst_.print());
  case TasKind:
    return fmt::format("TAS {}", dst_.print());
  case UnlinkKind:
    return fmt::format("UNLINK {}", dst_.print());
  case PeaKind:
    return fmt::format("PEA {}", src_.print());
  case TstKind:
    return fmt::format("TST.{} {}", format_size(), src_.print());
  case TrapKind:
    return fmt::format("TRAP Q");
  case NopKind:
    return "NOP";
  case ResetKind:
    return "RESET";
  case RteKind:
    return "RTE";
  case RtsKind:
    return "RTS";
  case RtrKind:
    return "RTR";
  case TrapvKind:
    return "TRAPV";
  }

#undef NAME

  return "";
}

} // namespace m68k

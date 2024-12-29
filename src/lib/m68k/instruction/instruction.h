#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/common/context.h"
#include "lib/m68k/target/target.h"

namespace m68k {

class Instruction {
public:
  // keep this enum sorted
  enum Kind : uint8_t {
    AbcdKind,        // ABCD
    AddKind,         // ADD
    AddaKind,        // ADDA
    AddiKind,        // ADDI
    AddqKind,        // ADDQ
    AddxKind,        // ADDX
    AndKind,         // AND
    AndiKind,        // ANDI
    AndiToCcrKind,   // ANDItoCCR
    AndiToSrKind,    // ANDItoSR
    AslKind,         // ASL
    AsrKind,         // ASR
    BccKind,         // Bcc
    BchgKind,        // BCHG
    BclrKind,        // BCLR
    BsetKind,        // BSET
    BsrKind,         // BSR
    BtstKind,        // BTST
    ChkKind,         // CHK
    ClrKind,         // CLR
    CmpKind,         // CMP
    CmpaKind,        // CMPA
    CmpiKind,        // CMPI
    CmpmKind,        // CMPM
    DbccKind,        // DBcc
    DivsKind,        // DIVS
    DivuKind,        // DIVU
    EorKind,         // EOR
    EoriKind,        // EORI
    EoriToCcrKind,   // EORItoCCR
    EoriToSrKind,    // EORItoSR
    ExgKind,         // EXG
    ExtKind,         // EXT
    JmpKind,         // JMP
    JsrKind,         // JSR
    LeaKind,         // LEA
    LinkKind,        // LINK
    LslKind,         // LSL
    LsrKind,         // LSR
    MoveFromSrKind,  // MOVEfromSR
    MoveFromUspKind, // MOVEfromUSP
    MoveKind,        // MOVE
    MoveToCcrKind,   // MOVEtoCCR
    MoveToSrKind,    // MOVEtoSR
    MoveToUspKind,   // MOVEfromUSP
    MoveaKind,       // MOVEA
    MovemKind,       // MOVEM
    MovepKind,       // MOVEP
    MoveqKind,       // MOVEQ
    MulsKind,        // MULS
    MuluKind,        // MULU
    NbcdKind,        // NBCD
    NegKind,         // NEG
    NegxKind,        // NEGX
    NopKind,         // NOP
    NotKind,         // NOT
    OrKind,          // OR
    OriKind,         // ORI
    OriToCcrKind,    // ORItoCCR
    OriToSrKind,     // ORItoSR
    PeaKind,         // PEA
    ResetKind,       // RESET
    RolKind,         // ROL
    RorKind,         // ROR
    RoxlKind,        // ROXL
    RoxrKind,        // ROXR
    RteKind,         // RTE
    RtrKind,         // RTR
    RtsKind,         // RTS
    SbcdKind,        // SBCD
    SccKind,         // Scc
    SubKind,         // SUB
    SubaKind,        // SUBA
    SubiKind,        // SUBI
    SubqKind,        // SUBQ
    SubxKind,        // SUBX
    SwapKind,        // SWAP
    TasKind,         // TAS
    TrapKind,        // TRAP
    TrapvKind,       // TRAPV
    TstKind,         // TST
    UnlinkKind,      // UNLINK
  };

  enum Size : uint8_t {
    ByteSize = 1,
    WordSize = 2,
    LongSize = 4,
  };

  enum Condition : uint8_t {
    TrueCond,           // T
    FalseCond,          // F
    HigherCond,         // HI
    LowerOrSameCond,    // LS
    CarryClearCond,     // CC
    CarrySetCond,       // CS
    NotEqualCond,       // NE
    EqualCond,          // EQ
    OverflowClearCond,  // VC
    OverflowSetCond,    // VS
    PlusCond,           // PL
    MinusCond,          // MI
    GreaterOrEqualCond, // GE
    LessThanCond,       // LT
    GreaterThanCond,    // GT
    LessOrEqualCond,    // LE
  };

  // builder methods
  Instruction& kind(Kind kind);
  Instruction& size(Size size);
  Instruction& condition(Condition cond);
  Instruction& src(Target target);
  Instruction& dst(Target target);
  Instruction& data(Long data);

  [[nodiscard]] std::optional<Error> execute(Context ctx);

  // helper methods
  std::string print() const;

  // static methods
  static std::expected<Instruction, Error> decode(Context ctx);

private:
  Kind kind_;
  Size size_;
  Condition cond_;
  Target src_;
  Target dst_;
  Long data_;

  bool has_src_;
  bool has_dst_;
};

static_assert(sizeof(Instruction) == 64);
static_assert(std::is_trivially_constructible_v<Instruction>);

} // namespace m68k

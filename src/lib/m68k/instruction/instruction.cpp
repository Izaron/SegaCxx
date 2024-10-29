#include "instruction.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <fmt/core.h>

#include "lib/common/memory/types.h"
#include "lib/m68k/target/target.h"

namespace m68k {

Instruction& Instruction::kind(Kind kind) {
  kind_ = kind;
  has_src_ = has_dst_ = false;
  return *this;
}

Instruction& Instruction::size(Size size) {
  size_ = size;
  return *this;
}

Instruction& Instruction::condition(Condition cond) {
  cond_ = cond;
  return *this;
}

Instruction& Instruction::src(Target target) {
  src_ = target;
  has_src_ = true;
  return *this;
}

Instruction& Instruction::dst(Target target) {
  dst_ = target;
  has_dst_ = true;
  return *this;
}

Instruction& Instruction::data(Word data) {
  data_ = data;
  return *this;
}

} // namespace m68k

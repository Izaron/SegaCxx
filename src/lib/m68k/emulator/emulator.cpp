#include "emulator.h"

#include <optional>

#include "lib/m68k/error/error.h"
#include "lib/m68k/opcodes/instructions.h"

namespace m68k {

std::optional<Error> emulate(Context ctx) {
  auto inst = Instruction::decode(ctx);
  if (!inst) {
    return inst.error();
  }
  return inst->execute(ctx);
}

} // namespace m68k

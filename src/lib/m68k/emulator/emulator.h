#pragma once
#include <optional>

#include "lib/m68k/error/error.h"
#include "lib/m68k/memory/device.h"
#include "lib/m68k/registers/registers.h"

namespace m68k {

struct Context {
  Registers& registers;
  Device& device;
};

std::optional<Error> emulate(Context ctx);

} // namespace m68k

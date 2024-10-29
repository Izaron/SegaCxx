#pragma once
#include "lib/common/memory/device.h"
#include "lib/m68k/registers/registers.h"

namespace m68k {

struct Context {
  Registers& registers;
  Device& device;
};

} // namespace m68k

#pragma once
#include "device.h"
#include "lib/m68k/memory/types.h"

namespace m68k {

class Bus : public Device {
public:
  virtual void add_device(Device* device, AddressRange range) = 0;
};

} // namespace m68k

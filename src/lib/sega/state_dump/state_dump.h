#pragma once
#include "lib/sega/memory/vdp_device.h"
#include <string_view>

namespace sega {

class StateDump {
public:
  StateDump(VdpDevice& vdp_device);

  void save_dump_to_file(std::string_view path) const;
  void apply_dump_from_file(std::string_view path);

private:
  VdpDevice& vdp_device_;
};

} // namespace sega

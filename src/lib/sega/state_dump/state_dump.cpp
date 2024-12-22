#include "state_dump.h"
#include "lib/common/memory/types.h"
#include "lib/sega/memory/vdp_device.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

namespace sega {

StateDump::StateDump(VdpDevice& vdp_device) : vdp_device_{vdp_device} {}

void StateDump::save_dump_to_file(std::string_view path) const {
  std::ofstream file{path.data()};
  const auto dump = vdp_device_.dump_state({});
  file.write(reinterpret_cast<const char*>(dump.data()), dump.size());
  spdlog::info("save dump to file: {}", path);
}

void StateDump::apply_dump_from_file(std::string_view path) {
  spdlog::info("read dump from file: {}", path);
  std::ifstream file{path.data()};
  std::vector<char> data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  vdp_device_.apply_state({}, {reinterpret_cast<Byte*>(data.data()), data.size()});
}

} // namespace sega

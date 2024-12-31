#include "lib/sega/executor/executor.h"
#include "lib/sega/gui/gui.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include <cassert>
#include <string_view>

namespace sega {

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::info);

  assert(argc == 2);
  const auto rom_path = std::string_view{argv[1]};
  Executor executor{rom_path};

  Gui gui{executor};
  if (!gui.setup()) {
    return 1;
  }
  gui.loop();
  return 0;
}

} // namespace sega

int main(int argc, char** argv) {
  return sega::main(argc, argv);
}

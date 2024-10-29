#include "rom_loader.h"
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

namespace sega {

std::vector<char> load_rom(std::string_view path) {
  std::ifstream file{path.data()};
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace sega

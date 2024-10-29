#include "registers.h"
#include "lib/common/memory/types.h"
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>

namespace m68k {

std::string dump(const Registers& r) {
  std::stringstream ss;
  ss << std::hex << std::uppercase;
  for (int i = 0; i < 7; ++i) {
    ss << "D" << i << " = " << r.d[i] << "\tA" << i << " = " << r.a[i] << "\n";
  }
  ss << "D7 = " << r.d[7] << "\n";
  ss << "USP = " << r.usp << "\n";
  ss << "SSP = " << r.ssp << "\n";
  ss << "PC = " << r.pc << "\n";

  ss << "SR: ";
  ss << "T = " << r.sr.trace << ", ";
  ss << "S = " << r.sr.supervisor << ", ";
  ss << "M = " << r.sr.master_switch << ", ";
  ss << "I = " << r.sr.interrupt_mask << ", ";
  ss << "X = " << r.sr.extend << ", ";
  ss << "N = " << r.sr.negative << ", ";
  ss << "Z = " << r.sr.zero << ", ";
  ss << "V = " << r.sr.overflow << ", ";
  ss << "C = " << r.sr.carry << "\n";

  return ss.str();
}

std::string dump_colored(const Registers& r) {
  std::stringstream ss;

  const auto make_name = [](std::string_view name) {
    return fmt::format(fmt::fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "{}", name);
  };

  const auto make_reg = [&make_name](std::string_view name, std::integral auto value) {
    constexpr auto size = std::is_same_v<decltype(value), bool> ? 1 : 2 * sizeof(value);
    const auto fmt_value = fmt::format(fmt::fg(fmt::color::crimson), "{:0{}x}", size, value);
    return fmt::format("{} = {}", make_name(name), fmt_value);
  };

  for (int i = 0; i < 7; ++i) {
    ss << make_reg(fmt::format("D{}", i), r.d[i]) << "\t" << make_reg(fmt::format("A{}", i), r.a[i]) << "\n";
  }
  ss << make_reg("D7", r.d[7]) << "\n";
  ss << make_reg("USP", r.usp) << "\n";
  ss << make_reg("SSP", r.ssp) << "\n";
  ss << make_reg("PC", r.pc) << "\n";
  ss << make_reg("SR", Word{r.sr}) << " [" << make_reg("T", r.sr.trace) << " " << make_reg("S", r.sr.supervisor) << " "
     << make_reg("M", r.sr.master_switch) << " " << make_reg("I", r.sr.interrupt_mask) << " "
     << make_reg("X", r.sr.extend) << " " << make_reg("N", r.sr.negative) << " " << make_reg("Z", r.sr.zero) << " "
     << make_reg("V", r.sr.overflow) << " " << make_reg("C", r.sr.carry) << "]\n";

  return ss.str();
}

} // namespace m68k

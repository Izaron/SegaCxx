#include "registers.h"
#include <ios>
#include <sstream>
#include <string>

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
  ss << "I = " << r.sr.interrupt << ", ";
  ss << "X = " << r.sr.extend << ", ";
  ss << "N = " << r.sr.negative << ", ";
  ss << "Z = " << r.sr.zero << ", ";
  ss << "V = " << r.sr.overflow << ", ";
  ss << "C = " << r.sr.carry << "\n";

  return ss.str();
}

} // namespace m68k

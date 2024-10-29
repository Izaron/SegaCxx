#pragma once
#include <array>
#include <cstdint>
#include <fmt/core.h>
#include <string>

#include "fmt/base.h"
#include "fmt/format.h"
#include "lib/common/memory/types.h"

namespace m68k {

struct Registers {
  /**
   * Data registers D0 - D7
   */
  std::array<Long, 8> d;

  /**
   * Address registers A0 - A6
   */
  std::array<Long, 7> a;

  /**
   * User stack pointer
   */
  Long usp;

  /**
   * Supervisor stack pointer
   */
  Long ssp;

  /**
   * Program counter
   */
  Long pc;

  /**
   * Status register
   */
  struct {
    // lower byte
    bool carry : 1;
    bool overflow : 1;
    bool zero : 1;
    bool negative : 1;
    bool extend : 1;
    bool : 3;

    // upper byte
    uint8_t interrupt_mask : 3;
    bool : 1;
    bool master_switch : 1;
    bool supervisor : 1;
    uint8_t trace : 2;

    decltype(auto) operator=(const Word& word) {
      *reinterpret_cast<Word*>(this) = word;
      return *this;
    }

    operator Word() const {
      return *reinterpret_cast<const Word*>(this);
    }
  } sr;
  static_assert(sizeof(sr) == sizeof(Word));

  /**
   * The stack pointer register depend on the supervisor flag
   */
  Long& stack_ptr() {
    return sr.supervisor ? ssp : usp;
  }
};

std::string dump(const Registers& registers);
std::string dump_colored(const Registers& registers);

} // namespace m68k

template<>
struct fmt::formatter<m68k::Registers> : formatter<std::string> {
  auto format(const m68k::Registers& registers, format_context& ctx) const {
    return formatter<std::string>::format(dump_colored(registers), ctx);
  }
};

#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/registers/registers.h"
#include "lib/sega/memory/controller_device.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/rom_loader/rom_loader.h"
#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace sega {

class Executor {
public:
  enum class Result {
    Executed,
    VblankInterrupt,
  };

  struct InstructionInfo {
    AddressType pc;
    DataView bytes;
    std::string description;
  };

public:
  Executor(std::string_view rom_path);
  ~Executor();
  [[nodiscard]] std::expected<Result, Error> execute_current_instruction();

  void set_game_speed(double game_speed);
  void reset_interrupt_time();
  InstructionInfo current_instruction_info();

  ControllerDevice& controller_device();
  const VdpDevice& vdp_device() const;
  const VectorTable& vector_table() const;
  const Metadata& metadata() const;
  const m68k::Registers& registers() const;

  void save_dump_to_file(std::string_view path) const;
  void apply_dump_from_file(std::string_view path);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sega

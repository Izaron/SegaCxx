#include "executor.h"
#include "interrupt_handler.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/types.h"
#include "lib/m68k/instruction/instruction.h"
#include "lib/m68k/registers/registers.h"
#include "lib/sega/memory/bus_device.h"
#include "lib/sega/memory/controller_device.h"
#include "lib/sega/memory/m68k_ram_device.h"
#include "lib/sega/memory/psg_device.h"
#include "lib/sega/memory/rom_device.h"
#include "lib/sega/memory/trademark_register_device.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/memory/ym2612_device.h"
#include "lib/sega/memory/z80_device.h"
#include "lib/sega/rom_loader/rom_loader.h"
#include "lib/sega/state_dump/state_dump.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>
#include <vector>

namespace sega {

class Executor::Impl {
public:
  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;

  Impl(std::string_view rom_path)
      : rom_{load_rom(rom_path)}, rom_device_{DataView{reinterpret_cast<const Byte*>(rom_.data()), rom_.size()}},
        vdp_device_{bus_}, interrupt_handler_{vector_table().vblank_pc.get(), registers_, bus_, vdp_device_},
        state_dump_{vdp_device_} {
    spdlog::info("loaded ROM file {}", rom_path);

    // setup bus devices
    const auto rom_address = metadata().rom_address;
    bus_.add_device({rom_address.begin.get(), rom_address.end.get()}, &rom_device_);
    bus_.add_device(&z80_ram_device_);
    bus_.add_device(&ym2612_device_);
    bus_.add_device(&controller_device_);
    bus_.add_device(&z80_controller_device_);
    bus_.add_device(&trademark_register_device_);
    bus_.add_device(&vdp_device_);
    bus_.add_device(&psg_device_);
    bus_.add_device(&m68k_ram_device_);

    // make registers
    std::memset(&registers_, 0, sizeof(registers_));
    registers_.usp = vector_table().reset_sp.get();
    registers_.pc = vector_table().reset_pc.get();
  }

  [[nodiscard]] std::expected<Executor::Result, Error> execute_single_instruction() {
    // check if interrupt happened
    auto interrupt_check = interrupt_handler_.check();
    if (!interrupt_check.has_value()) {
      spdlog::error("interrupt error");
      return std::unexpected{std::move(interrupt_check.error())};
    }
    if (interrupt_check.value()) {
      return Executor::Result::VblankInterrupt;
    }

    // decode and execute the current instruction
    const auto begin_pc = registers_.pc;
    auto inst = m68k::Instruction::decode({.registers = registers_, .device = bus_});
    assert(inst);
    if (auto err = inst->execute({.registers = registers_, .device = bus_})) {
      spdlog::error("execute error pc: {:06x} what: {}", begin_pc, err->what());
      return std::unexpected{std::move(*err)};
    }
    return Executor::Result::Executed;
  }

  void set_game_speed(double game_speed) {
    interrupt_handler_.set_game_speed(game_speed);
  }

  void reset_interrupt_time() {
    interrupt_handler_.reset_time();
  }

  Executor::InstructionInfo current_instruction_info() {
    // print current instruction, therefore double-fetching it, so need to restore PC
    const auto begin_pc = registers_.pc;
    auto inst = m68k::Instruction::decode({.registers = registers_, .device = bus_});
    assert(inst);
    const auto end_pc = registers_.pc;
    registers_.pc = begin_pc;

    return {.pc = begin_pc,
            .bytes = DataView{reinterpret_cast<const Byte*>(rom_.data() + begin_pc), end_pc - begin_pc},
            .description = inst->print()};
  }

  ControllerDevice& controller_device() {
    return controller_device_;
  }

  const VdpDevice& vdp_device() const {
    return vdp_device_;
  }

  const VectorTable& vector_table() const {
    return rom_header().vector_table;
  }

  const Metadata& metadata() const {
    return rom_header().metadata;
  }

  const m68k::Registers& registers() const {
    return registers_;
  }

  void save_dump_to_file(std::string_view path) const {
    state_dump_.save_dump_to_file(path);
  }

  void apply_dump_from_file(std::string_view path) {
    state_dump_.apply_dump_from_file(path);
  }

private:
  const Header& rom_header() const {
    return *reinterpret_cast<const Header*>(rom_.data());
  }

private:
  // ROM content
  const std::vector<char> rom_;

  // memory devices
  BusDevice bus_;
  RomDevice rom_device_;
  Z80RamDevice z80_ram_device_;
  Ym2612Device ym2612_device_;
  ControllerDevice controller_device_;
  Z80ControllerDevice z80_controller_device_;
  TrademarkRegisterDevice trademark_register_device_;
  VdpDevice vdp_device_;
  PsgDevice psg_device_;
  M68kRamDevice m68k_ram_device_;

  // registers
  m68k::Registers registers_;

  // interrupt handler
  InterruptHandler interrupt_handler_;

  // utils
  StateDump state_dump_;
};

Executor::Executor(std::string_view rom_path) : impl_{std::make_unique<Impl>(rom_path)} {}

Executor::~Executor() = default;

[[nodiscard]] std::expected<Executor::Result, Error> Executor::execute_current_instruction() {
  return impl_->execute_single_instruction();
}

void Executor::set_game_speed(double game_speed) {
  impl_->set_game_speed(game_speed);
}

void Executor::reset_interrupt_time() {
  impl_->reset_interrupt_time();
}

Executor::InstructionInfo Executor::current_instruction_info() {
  return impl_->current_instruction_info();
}

ControllerDevice& Executor::controller_device() {
  return impl_->controller_device();
}

const VdpDevice& Executor::vdp_device() const {
  return impl_->vdp_device();
}

const VectorTable& Executor::vector_table() const {
  return impl_->vector_table();
}

const Metadata& Executor::metadata() const {
  return impl_->metadata();
}

const m68k::Registers& Executor::registers() const {
  return impl_->registers();
}

void Executor::save_dump_to_file(std::string_view path) const {
  return impl_->save_dump_to_file(path);
}

void Executor::apply_dump_from_file(std::string_view path) {
  return impl_->apply_dump_from_file(path);
}

} // namespace sega

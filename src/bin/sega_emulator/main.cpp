#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fmt/color.h>
#include <fmt/core.h>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "lib/common/memory/types.h"
#include "lib/m68k/common/context.h"
#include "lib/m68k/instruction/instruction.h"
#include "lib/m68k/registers/registers.h"
#include "lib/sega/executor/executor.h"
#include "lib/sega/executor/interrupt_handler.h"
#include "lib/sega/gui/gui.h"
#include "lib/sega/memory/bus_device.h"
#include "lib/sega/memory/controller_device.h"
#include "lib/sega/memory/m68k_ram_device.h"
#include "lib/sega/memory/psg_device.h"
#include "lib/sega/memory/rom_device.h"
#include "lib/sega/memory/trademark_register_device.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/memory/z80_device.h"
#include "lib/sega/rom_loader/rom_loader.h"

namespace sega {

void log_rom_metadata(const Metadata& metadata) {
  spdlog::info("system type: \"{}\"", std::string_view{metadata.system_type});
  spdlog::info("copyright: \"{}\"", std::string_view{metadata.copyright});
  spdlog::info("domestic_title: \"{}\"", std::string_view{metadata.domestic_title});
  spdlog::info("overseas_title: \"{}\"", std::string_view{metadata.overseas_title});
  spdlog::info("serial_number: \"{}\"", std::string_view{metadata.serial_number});
  spdlog::info("checksum: {:04x}", metadata.checksum.get());
  spdlog::info("device_support: \"{}\"", std::string_view{metadata.device_support});
  spdlog::info("rom_address: [{:06x}, {:06x}]", metadata.rom_address.begin.get(), metadata.rom_address.end.get());
  spdlog::info("ram_address: [{:06x}, {:06x}]", metadata.ram_address.begin.get(), metadata.ram_address.end.get());
  spdlog::info("extra_memory: \"{}\"", std::string_view{metadata.extra_memory});
  spdlog::info("modem_support: \"{}\"", std::string_view{metadata.modem_support});
  spdlog::info("region_support: \"{}\"", std::string_view{metadata.region_support});
}

std::string dump_cram(std::span<const Byte> cram) {
  constexpr auto convert = [](int value) -> uint8_t { return value * 255 / 0xE; };
  std::stringstream ss;
  for (size_t i = 0; i < cram.size(); i += 2) {
    const auto blue = convert(cram[i] & 0xF);
    const auto green = convert((cram[i + 1] & 0xF0) >> 4);
    const auto red = convert(cram[i + 1] & 0xF);
    ss << fmt::format(fmt::bg(fmt::rgb{red, green, blue}) | fmt::emphasis::bold, "  ");
    if ((i + 2) % 32 == 0) {
      ss << "\n";
    }
  }
  return ss.str();
}

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

  // load ROM and log metadata
  const auto rom = load_rom(rom_path);
  const auto& header = *reinterpret_cast<const Header*>(rom.data());
  const auto& vector_table = header.vector_table;
  const auto& metadata = header.metadata;
  log_rom_metadata(metadata);

  // make devices
  BusDevice bus;

  RomDevice rom_device{DataView{reinterpret_cast<const Byte*>(rom.data()), rom.size()}};
  Z80RamDevice z80_ram_device{};
  ControllerDevice controller_device{};
  Z80ControllerDevice z80_controller_device{};
  TrademarkRegisterDevice trademark_register_device{};
  VdpDevice vdp_device{bus};
  PsgDevice psg_device{};
  M68kRamDevice m68k_ram_device{};

  bus.add_device({metadata.rom_address.begin.get(), metadata.rom_address.end.get()}, &rom_device);
  bus.add_device({Z80RamDevice::kBegin, Z80RamDevice::kEnd}, &z80_ram_device);
  bus.add_device({ControllerDevice::kBegin, ControllerDevice::kEnd}, &controller_device);
  bus.add_device({Z80ControllerDevice::kBegin, Z80ControllerDevice::kEnd}, &z80_controller_device);
  bus.add_device({TrademarkRegisterDevice::kBegin, TrademarkRegisterDevice::kEnd}, &trademark_register_device);
  bus.add_device({VdpDevice::kBegin, VdpDevice::kEnd}, &vdp_device);
  bus.add_device({PsgDevice::kBegin, PsgDevice::kEnd}, &psg_device);
  bus.add_device({M68kRamDevice::kBegin, M68kRamDevice::kEnd}, &m68k_ram_device);

  // make registers
  m68k::Registers registers;
  std::memset(&registers, 0, sizeof(registers));
  registers.usp = vector_table.reset_sp.get();
  registers.pc = vector_table.reset_pc.get();

  // make interrupt handler
  InterruptHandler interrupt_handler{vector_table.vblank_pc.get(), registers, bus, vdp_device};

  // make context
  m68k::Context ctx{.registers = registers, .device = bus};

  // run emulation within a simple "debugger"
  const auto run_until = [&](auto&& condition) -> bool {
    interrupt_handler.reset_time();
    size_t prek = 0;
    while (!condition()) {
      // check if interrupt should be invoked
      const auto interrupt_check = interrupt_handler.check();
      if (!interrupt_check.has_value()) {
        spdlog::error("interrupt error kind: {} what: {}", magic_enum::enum_name(interrupt_check.error().kind()),
                      interrupt_check.error().what());
        return false;
      }
      if (interrupt_check.value()) {
        continue;
      }

      // check a regular instruction
      const auto begin_pc = registers.pc;
      auto inst = m68k::Instruction::decode(ctx);
      assert(inst);
      const auto err = inst->execute(ctx);
      if (err) {
        spdlog::error("execute error kind: {} what: {} pc: {:06x}", magic_enum::enum_name(err->kind()), err->what(),
                      begin_pc);
        return false;
      }

      ++prek;
      if (prek >= 0x4000) {
        prek = 0;
        std::cerr << ">> CRAM colors:\n" << dump_cram(vdp_device.cram_data()) << std::flush;
      }
    }
    return true;
  };

  std::string prev_command;
  while (true) {
    // print current instruction, therefore double-fetching it, so need to restore PC
    const auto begin_pc = registers.pc;
    auto inst = m68k::Instruction::decode(ctx);
    if (!inst) {
      spdlog::error("decode error kind: {} what: {}", magic_enum::enum_name(inst.error().kind()), inst.error().what());
      break;
    }
    const auto end_pc = registers.pc;
    registers.pc = begin_pc;
    spdlog::debug("now at address: {:08x} bytes: {} instruction: {}", begin_pc,
                  DataView{reinterpret_cast<const Byte*>(rom.data() + begin_pc), end_pc - begin_pc}, inst->print());

    // parse the command
    std::string command;
    std::cerr << ">> " << std::flush;
    std::getline(std::cin, command);
    if (command.empty()) {
      command = prev_command;
    }
    prev_command = command;

    if (command == "h") {
      std::cerr << ">> debugger commands:" << std::endl;
#define COMMAND(command, description)                                                                                  \
  std::cerr << ">>\t\"" << fmt::format(fmt::emphasis::bold, command) << "\" - " << (description) << std::endl;
      COMMAND("h", "print this help")
      COMMAND("r", "print registers")
      COMMAND("s", "execute a single instruction")
      COMMAND("n", "execute until next instruction")
      COMMAND("c", "execute forever")
      COMMAND("a ADDR", "execute until address \"ADDR\"")
      COMMAND("cram", "dump CRAM content")
      COMMAND("q", "quit")
#undef COMMAND
    } else if (command == "r") {
      std::cerr << fmt::format(">> registers:\n{}", registers) << std::endl;
    } else if (command == "s") {
      if (!run_until([cnt = 0] mutable { return cnt++ > 0; })) {
        break;
      }
    } else if (command == "n") {
      if (!run_until([&registers, end_pc] { return registers.pc == end_pc; })) {
        break;
      }
    } else if (command == "c") {
      if (!run_until([] { return false; })) {
        break;
      }
    } else if (command.starts_with("a ")) {
      // not beautiful parsing of a hex value...
      std::stringstream ss;
      ss << std::hex << command.substr(2);
      Long target_pc;
      ss >> target_pc;
      if (!run_until([&registers, target_pc] { return registers.pc == target_pc; })) {
        break;
      }
    } else if (command == "cram") {
      std::cerr << ">> CRAM colors:\n" << dump_cram(vdp_device.cram_data()) << std::flush;
    } else if (command == "q") {
      std::cerr << ">> quit" << std::endl;
      break;
    } else {
      std::cerr << ">> unknown command" << std::endl;
    }
  }
  spdlog::info("simulation finished");
  return 0;
}

} // namespace sega

int main(int argc, char** argv) {
  return sega::main(argc, argv);
}

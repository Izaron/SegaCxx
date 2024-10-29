add_executable(sega_emulator main.cpp)
target_link_libraries(
    sega_emulator
    sega_executor
    sega_memory
    sega_rom_loader
    m68k_instruction
    m68k_target
    m68k_registers
    sega_gui
)

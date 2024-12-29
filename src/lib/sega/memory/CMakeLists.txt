add_library(
    sega_memory
    bus_device.cpp
    controller_device.cpp
    m68k_ram_device.cpp
    psg_device.cpp
    rom_device.cpp
    trademark_register_device.cpp
    vdp_device.cpp
    ym2612_device.cpp
    z80_device.cpp
)
target_link_libraries(sega_memory memory spdlog::spdlog_header_only fmt::fmt-header-only)

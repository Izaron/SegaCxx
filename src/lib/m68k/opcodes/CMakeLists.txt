add_library(m68k_opcodes STATIC
    instructions.cpp
    targets.cpp
)

target_link_libraries(m68k_opcodes m68k_registers)

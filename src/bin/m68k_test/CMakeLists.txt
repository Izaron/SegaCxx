add_executable(m68k_test main.cpp)
target_link_libraries(m68k_test m68k_registers m68k_instruction memory error)

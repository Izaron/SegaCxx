add_library(m68k_instruction decode.cpp execute.cpp instruction.cpp print.cpp)
target_link_libraries(m68k_instruction m68k_target fmt::fmt-header-only)

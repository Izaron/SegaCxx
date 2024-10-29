add_library(sega_executor executor.cpp interrupt_handler.cpp)
target_link_libraries(sega_executor sega_memory spdlog::spdlog_header_only)

add_library(sega_gui gui.cpp)
target_link_libraries(
    sega_gui
    sega_video
    sega_shader
    spdlog::spdlog_header_only
    imgui
)

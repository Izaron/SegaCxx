add_library(sega_gui gui.cpp)
target_link_libraries(
    sega_gui
    sega_video
    spdlog::spdlog_header_only
    imgui
    SDL2
    ${OPENGL_LIBRARIES}
)

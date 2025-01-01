add_library(sega_shader shader.cpp)
target_link_libraries(
    sega_shader
    spdlog::spdlog_header_only
    glad_gl_core_3_0
    glfw
    ${OPENGL_LIBRARIES}
)

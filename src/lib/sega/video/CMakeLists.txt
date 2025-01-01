add_library(sega_video colors.cpp tilemap.cpp sprite_table.cpp video.cpp plane.cpp)
target_link_libraries(
    sega_video
    sega_image_saver
    spdlog::spdlog_header_only
    ${OPENGL_LIBRARIES}
)

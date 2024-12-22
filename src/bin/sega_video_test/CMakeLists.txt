add_executable(sega_video_test main.cpp)
target_link_libraries(
    sega_video_test
    sega_memory
    sega_state_dump
    sega_video
)

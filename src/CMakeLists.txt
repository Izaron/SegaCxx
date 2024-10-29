cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_COMPILER "clang++")
project(segacxx VERSION 0.1 LANGUAGES CXX)

# compiler settings
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_compile_options(-Wno-format)
add_compile_options(-fno-exceptions)
add_compile_options(-g)
add_compile_options(-fsanitize=undefined,address)
add_link_options(-fsanitize=undefined,address)

# add local packages
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
find_package(OpenGL REQUIRED)

# add external packages
cmake_policy(SET CMP0135 NEW)
include(FetchContent)

# external package: fmt
FetchContent_Declare(fmt_external
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(fmt_external)
include_directories(${fmt_external_SOURCE_DIR}/include)

# external package: spdlog
add_compile_definitions(SPDLOG_FMT_EXTERNAL)
add_compile_definitions(SPDLOG_NO_EXCEPTIONS)
FetchContent_Declare(spdlog_external
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(spdlog_external)
include_directories(${spdlog_external_SOURCE_DIR}/include)

# external package: imgui
FetchContent_Declare(imgui_external
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.4.tar.gz
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(imgui_external)
add_library(imgui
	${imgui_external_SOURCE_DIR}/imgui.cpp
	${imgui_external_SOURCE_DIR}/imgui_demo.cpp
	${imgui_external_SOURCE_DIR}/imgui_draw.cpp
	${imgui_external_SOURCE_DIR}/imgui_tables.cpp
	${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
include_directories(${imgui_external_SOURCE_DIR})
include_directories(${imgui_external_SOURCE_DIR}/backends)
target_include_directories(imgui PUBLIC ${imgui_external_SOURCE_DIR})

# external package: nlohmann
FetchContent_Declare(nlohmann_external
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(nlohmann_external)
include_directories(${nlohmann_external_SOURCE_DIR}/single_include)

# external package: magic_enum
FetchContent_Declare(magic_enum_external
    URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.6.tar.gz
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(magic_enum_external)
include_directories(${magic_enum_external_SOURCE_DIR}/include)

# add subdirectories
add_subdirectory(bin)
add_subdirectory(lib)

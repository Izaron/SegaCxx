cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_COMPILER "clang++")
project(segacxx VERSION 0.1 LANGUAGES CXX)

# compiler settings
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_compile_options(-fno-exceptions)
#add_compile_options(-g)
#add_compile_options(-fsanitize=undefined)
#add_link_options(-fsanitize=undefined)

# add subdirectories
add_subdirectory(lib)

# enable googletest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.0
)
FetchContent_MakeAvailable(googletest)
enable_testing()

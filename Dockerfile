FROM ubuntu:latest

RUN apt-get update

# build packages
RUN apt-get install -y cmake clang ninja-build git libc++-dev libc++abi-dev

# library packages
RUN apt-get install -y libspdlog-dev 

# vim packages
RUN apt-get install -y clangd clang-format neovim

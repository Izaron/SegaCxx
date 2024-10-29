FROM ubuntu:latest

RUN apt-get update

# build packages
RUN apt-get install -y cmake clang ninja-build git libc++-dev libc++abi-dev

# development packages
RUN apt-get install -y clangd clang-format neovim gdb htop strace python3-pil byobu

# library packages
RUN apt-get install -y libsdl2-dev language-pack-en

FROM ubuntu:latest

RUN apt-get update --fix-missing

# build packages
RUN apt-get install -y cmake clang ninja-build git pkg-config python3-jinja2 language-pack-en libc++-dev libc++abi-dev libwayland-dev mesa-common-dev xorg-dev libxkbcommon-dev

# development packages
RUN apt-get install -y clangd clang-format neovim gdb htop strace python3-pil byobu

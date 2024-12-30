#pragma once
#include <utility>

[[noreturn]] inline void unreachable() {
  std::unreachable();
}

#include "error.h"
#include <cstdarg>
#include <cstdio>
#include <string>

namespace m68k {

Error::Error(Kind kind, const char* format, ...) : kind_{kind} {
  char buffer[256];
  std::va_list args;
  va_start(args, format);
  vsnprintf(buffer, 256, format, args);
  va_end(args);

  what_ = std::string{buffer};
}

} // namespace m68k

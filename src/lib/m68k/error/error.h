#pragma once
#include <string>

namespace m68k {

class Error {
public:
  enum Kind {
    Ok,
    UnalignedMemoryRead,
    UnalignedMemoryWrite,
    UnalignedProgramCounter,
    UnknownAddressingMode,
    UnknownOpcode,
  };

  Error() = default;
  Error(Kind type, const char* format, ...);

  Kind kind() const {
    return kind_;
  }
  const std::string& what() const {
    return what_;
  }

private:
  Kind kind_{Ok};
  std::string what_;
};

} // namespace m68k

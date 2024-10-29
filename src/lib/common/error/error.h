#pragma once
#include <string>
#include <utility>

class Error {
public:
  enum Kind {
    // no error
    Ok,

    UnalignedMemoryRead,
    UnalignedMemoryWrite,
    UnalignedProgramCounter,
    UnknownAddressingMode,
    UnknownOpcode,

    // permission error
    ProtectedRead,
    ProtectedWrite,

    // bus error
    UnmappedRead,
    UnmappedWrite,

    // invalid action
    InvalidRead,
    InvalidWrite,
  };

  Error() = default;
  Error(Kind kind, std::string what): kind_{kind}, what_{std::move(what)}
  {}

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

#pragma once

template<typename T>
class Passkey {
  friend T;
  Passkey() = default;
};

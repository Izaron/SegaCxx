#pragma once
#include <glad/gl.h>

#include "magic_enum/magic_enum.hpp"
#include <GL/gl.h>
#include <array>

namespace sega {

enum class ShaderType {
  Nothing,
  Crt,
  Desaturate,
  Glitch,
  NightVision,
};

class Shader {
public:
  void build_programs();
  GLuint get_program(ShaderType shader_type) const;

private:
  static constexpr auto kShaderCount = magic_enum::enum_count<ShaderType>();
  std::array<GLuint, kShaderCount> programs_;
};

} // namespace sega

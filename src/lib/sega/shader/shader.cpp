#include "shader.h"
#include "glad/gl.h"
#include "spdlog/spdlog.h"
#include <array>
#include <string_view>
#include <utility>

namespace sega {

namespace {

constexpr std::string_view kVertexShaderSource = R"(
#version 130
uniform mat4 ProjMtx;
in vec2 Position;
in vec2 UV;
in vec4 Color;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy,0,1);
}
)";

constexpr std::string_view kFragmentShaderNothingSource = R"(
#version 130
uniform sampler2D Texture;
in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 Out_Color;
void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
)";

constexpr std::string_view kFragmentShaderCrtSource = R"(
#version 130

uniform sampler2D Texture;

// ----------------------------------------------------
// constants for Barrel Distortion + Scan Lines + Chromatic Aberration + Vignette
// ----------------------------------------------------
const float DistortionStrength = 0.13;   // Barrel distortion amount
const float ScanlineDarkness   = 0.25;   // Darken for scanlines
const float ChromaticOffset   = 0.002;   // Shift in UV for R/B channels
const float VignetteStrength  = 0.3;     // How strongly edges are darkened

in vec2 Frag_UV;
in vec4 Frag_Color;
out vec4 Out_Color;

void main()
{
    // ----------------------------------------------------
    // 1) Barrel Distortion
    // ----------------------------------------------------
    // Shift UV from [0..1] to [-1..1], so (0.5,0.5) is (0,0)
    vec2 centerUV = Frag_UV * 2.0 - 1.0;

    // Distance from center squared
    float r2 = dot(centerUV, centerUV);

    // Simple barrel distortion: newCoord = oldCoord * (1 + k*r^2)
    vec2 distortedUV = centerUV * (1.0 + DistortionStrength * r2);

    // Shift back to [0..1] space
    distortedUV = (distortedUV + 1.0) * 0.5;

    // ----------------------------------------------------
    // 2) Black out pixels that fall outside [0..1]
    // ----------------------------------------------------
    if (distortedUV.x < 0.0 || distortedUV.x > 1.0 ||
        distortedUV.y < 0.0 || distortedUV.y > 1.0)
    {
        Out_Color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // ----------------------------------------------------
    // 3) Chromatic Aberration
    // ----------------------------------------------------
    // We offset the R and B channels slightly to emulate color fringing.
    // G channel uses the original distortedUV.
    // For each channel, if the UV goes out of range, that channel is black.
    vec2 uvR = distortedUV + vec2(-ChromaticOffset, 0.0);
    vec2 uvG = distortedUV;
    vec2 uvB = distortedUV + vec2( ChromaticOffset, 0.0);

    float r = 0.0;
    float g = 0.0;
    float b = 0.0;

    // Sample R channel if in range
    if (uvR.x >= 0.0 && uvR.x <= 1.0 &&
        uvR.y >= 0.0 && uvR.y <= 1.0)
    {
        r = texture(Texture, uvR).r;
    }

    // Sample G channel if in range
    if (uvG.x >= 0.0 && uvG.x <= 1.0 &&
        uvG.y >= 0.0 && uvG.y <= 1.0)
    {
        g = texture(Texture, uvG).g;
    }

    // Sample B channel if in range
    if (uvB.x >= 0.0 && uvB.x <= 1.0 &&
        uvB.y >= 0.0 && uvB.y <= 1.0)
    {
        b = texture(Texture, uvB).b;
    }

    // Combine R, G, B into final color (multiplied by vertex color)
    vec4 color = Frag_Color * vec4(r, g, b, 1.0);

    // ----------------------------------------------------
    // 4) Vignette
    // ----------------------------------------------------
    // Reuse centerUV for a radial darkening from center (0,0).
    // Because centerUV is already in [-1..1], we can use length(centerUV).
    float distFromCenter = length(centerUV);
    // The larger distFromCenter is, the darker the pixel becomes.
    // This formula leaves center fully lit, edges dark.
    float vignette = 1.0 - clamp(distFromCenter * VignetteStrength, 0.0, 1.0);
    color.rgb *= vignette;

    // ----------------------------------------------------
    // 5) Scan Lines (darken every other horizontal line)
    // ----------------------------------------------------
    float line = mod(gl_FragCoord.y, 2.0);
    float brightnessFactor = 1.0 - (line < 1.0 ? ScanlineDarkness : 0.0);
    color.rgb *= brightnessFactor;

    // Done
    Out_Color = color;
}
)";

constexpr std::string_view kFragmentShaderDesaturateSource = R"(
#version 130

uniform sampler2D Texture;

const float DesaturateFactor = 0.7;  // 0.0 => full color, 1.0 => fully grayscale
const float GrainStrength    = 0.05; // How much random grain is added

in vec2  Frag_UV;
in vec4  Frag_Color;
out vec4 Out_Color;

// A quick pseudo-random generator based on gl_FragCoord
// Uses a “hash” trick with sin and dot to generate noise in [0..1].
float random2(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // 1) Sample the original texture color (no geometric distortion)
    vec4 color = texture(Texture, Frag_UV) * Frag_Color;

    // 2) Partial Desaturation
    //    We convert the color towards grayscale, based on DesaturateFactor.
    //    For a standard luminance approach, we do:
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114)); 
    //    'gray' is a single brightness value. Then we interpolate:
    color.rgb = mix(color.rgb, vec3(gray), DesaturateFactor);
    //    If DesaturateFactor=0.0 => unchanged color;
    //       DesaturateFactor=1.0 => black & white.

    // 3) Film Grain
    //    Generate a per-pixel noise value using gl_FragCoord.
    //    Range is [0..1]. We shift it so that 0.5 is “no change.”
    float grain = random2(gl_FragCoord.xy) - 0.5; // range ~[-0.5..0.5]
    //    Scale by GrainStrength and add it to the color.
    color.rgb += grain * GrainStrength;

    // 4) Clamp final color so it stays in [0..1]
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    // 5) Output the final color
    Out_Color = color;
}
)";

constexpr std::string_view kFragmentShaderGlitchSource = R"(
#version 130

uniform sampler2D Texture;

const float GlitchStripeSize = 8.0;   // Stripe height in pixels
const float GlitchIntensity  = 0.2;   // Chance that a stripe "glitches"
const float InvertChance     = 0.3;   // Within a glitch, chance to invert color
const float ShiftChance      = 0.4;   // Within a glitch, chance to swap color channels
const float ColorBoost       = 1.1;   // Slight brightness/contrast lift

in vec2  Frag_UV;
in vec4  Frag_Color;
out vec4 Out_Color;

// A small pseudo-random function using a hash of sin/dot:
float random2(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // 1) Sample the original, undistorted texture color
    vec4 color = texture(Texture, Frag_UV) * Frag_Color;

    // 2) Slight brightness/contrast boost (futuristic punch)
    //    You can remove or adjust these lines if desired
    //    Center around 0.5 for contrast, then re-center
    color.rgb = (color.rgb - 0.5) * 1.2 + 0.5; 
    color.rgb *= ColorBoost;

    // 3) Determine which horizontal stripe we’re in (integer)
    //    We treat gl_FragCoord.y / GlitchStripeSize as an index
    float stripeIndex = floor(gl_FragCoord.y / GlitchStripeSize);

    // 4) Generate a random value for this stripe
    //    We include gl_FragCoord.x as well so each pixel in the stripe
    //    can differ slightly (but share some correlation with the stripe)
    float rng = random2(vec2(stripeIndex, gl_FragCoord.x));

    // 5) Decide if this stripe is “glitchy” based on GlitchIntensity
    //    If rng < GlitchIntensity, we do some glitch effects
    if (rng < GlitchIntensity)
    {
        // 5a) Another random to decide how we glitch
        float rng2 = random2(vec2(stripeIndex * 0.37, gl_FragCoord.x * 0.11));

        // 5b) Maybe invert color (if rng2 < InvertChance)
        if (rng2 < InvertChance) {
            color.rgb = 1.0 - color.rgb; 
        } 
        // 5c) Or maybe shift color channels around (if rng2 >= InvertChance && rng2 < InvertChance+ShiftChance)
        else if (rng2 < InvertChance + ShiftChance) {
            // For example, swap Red <-> Blue
            color.rgb = color.bgr;
        }
        // Otherwise do nothing (still “glitchy,” but no specific effect)
    }

    // 6) Clamp the final color so it stays in [0..1]
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    // 7) Output final color
    Out_Color = color;
}
)";

constexpr std::string_view kFragmentShaderNightVisionSource = R"(
#version 130

uniform sampler2D Texture;

// The night-vision green tint
const vec3  NightVisionColor = vec3(0.1, 0.95, 0.2);

// How strong the random noise overlay is
const float NoiseStrength = 0.02;

// Vignetting: how quickly the edges fade to black
// Larger => more fade near the edges
const float VignetteStrength = 2.0;

// Simple brightness and contrast adjustments
const float BaseBrightness   = 1.05;
const float BaseContrast     = 1.2;

in vec2  Frag_UV;
in vec4  Frag_Color;
out vec4 Out_Color;

// A small pseudo-random generator based on sin/dot
float random2(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    // 1) Sample the texture at Frag_UV (no geometric distortion)
    vec4 color = texture(Texture, Frag_UV) * Frag_Color;

    // 2) Convert to grayscale first, for the classic night-vision look
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));

    // 3) Apply brightness/contrast to the grayscale
    float adjGray = (gray - 0.5) * BaseContrast + 0.5;
    adjGray      *= BaseBrightness;
    // clamp in case it goes out of [0..1]
    adjGray       = clamp(adjGray, 0.0, 1.0);

    // 4) Tint it green: multiply the grayscale by the night vision color
    vec3 tintedColor = adjGray * NightVisionColor;

    // 5) Add random noise
    // Use gl_FragCoord as a seed, range ~[-0.5..+0.5], scaled by NoiseStrength
    float n = random2(gl_FragCoord.xy) - 0.5;
    tintedColor += n * NoiseStrength;

    // 6) Vignetting
    // We measure how far from the center we are, then darken edges.
    // Let's define the center as (0.5, 0.5) in UV space.
    vec2  centerUV = Frag_UV - vec2(0.5, 0.5);
    float dist     = length(centerUV);
    // The further from center, the more we darken
    float vignetteFactor = 1.0 - clamp(dist * VignetteStrength, 0.0, 1.0);
    tintedColor *= vignetteFactor;

    // 7) Final clamp to ensure [0..1]
    tintedColor = clamp(tintedColor, 0.0, 1.0);

    // 8) Output final color (keep alpha from original if needed)
    Out_Color = vec4(tintedColor, color.a);
}
)";

GLuint compile_shader(GLenum shaderType, const char* source) {
  GLuint shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, 512, nullptr, log);
    spdlog::error("shader compilation error: {}", log);
  }
  return shader;
}

GLuint build_shader_program(std::string_view vertex_shader_source, std::string_view fragment_shader_source) {
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(program, 512, nullptr, log);
    spdlog::error("program linking error: {}", log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return program;
}

GLuint build_shader_program(std::string_view fragment_shader_source) {
  return build_shader_program(kVertexShaderSource, fragment_shader_source);
}

} // namespace

void Shader::build_programs() {
  static const std::array<std::pair<ShaderType, std::string_view>, kShaderCount> kMap = {
      std::make_pair(ShaderType::Nothing, kFragmentShaderNothingSource),
      std::make_pair(ShaderType::Crt, kFragmentShaderCrtSource),
      std::make_pair(ShaderType::Desaturate, kFragmentShaderDesaturateSource),
      std::make_pair(ShaderType::Glitch, kFragmentShaderGlitchSource),
      std::make_pair(ShaderType::NightVision, kFragmentShaderNightVisionSource),
  };
  for (const auto [shader_type, source] : kMap) {
    programs_[std::to_underlying(shader_type)] = build_shader_program(source);
  }
}

GLuint Shader::get_program(ShaderType shader_type) const {
  return programs_[std::to_underlying(shader_type)];
}

} // namespace sega

#pragma once
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "lib/sega/executor/executor.h"
#include "lib/sega/shader/shader.h"
#include "lib/sega/video/plane.h"
#include "lib/sega/video/sprite_table.h"
#include "lib/sega/video/tilemap.h"
#include "lib/sega/video/video.h"
#include <GL/gl.h>
#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace sega {

class Gui {
public:
  Gui(Executor& executor);
  ~Gui();

  bool setup();
  void loop();

private:
  // Poll events, returns false if should stop
  bool poll_events();

  // Set info about pressed buttons
  void update_controller();

  // Execute instructions while conditions is met or there is a VBlank
  void execute();

  // Render whole screen
  void render();

  // Main window
  void add_main_window();

  // Game window
  void add_game_window();

  // Execution window
  void add_execution_window();
  void add_execution_window_statistics();
  void add_execution_window_instruction_info();
  void add_execution_window_commands();
  void add_execution_window_registers();

  // Colors window
  void add_colors_window();

  // Tilemap window
  void add_tilemap_window();

  // Plane window
  void add_plane_window(PlaneType plane_type);

  // Sprite table window
  void add_sprite_table_window();

private:
  Executor& executor_;
  GLFWwindow* window_{};

  // Shader variables
  Shader shader_;
  GLuint shader_program_;
  ShaderType current_shader_type_{ShaderType::Crt};

  // Game window
  bool show_game_window_{true};
  int game_scale_{1};
  Video video_;

  // Execution window
  bool show_execution_window_{true};
  std::array<char, 7> until_address_{};
  std::function<bool()> condition_;
  uint64_t executed_count_{};

  // Colors window
  bool show_colors_window_{false};

  // Tilemap window
  bool show_tilemap_window_{false};
  int tilemap_scale_{1};
  int tilemap_palette_{};
  Tilemap tilemap_;

  // Plane A / Plane B / Window planes
  std::array<bool, kPlaneTypes> show_plane_window_{};
  std::array<int, kPlaneTypes> plane_scale_{1, 1, 1};
  std::array<Plane, kPlaneTypes> planes_;

  // Sprite table window
  bool show_sprite_table_window_{false};
  bool sprite_table_auto_update_{false};
  int sprite_scale_{1};
  std::span<const Sprite> sprites_;
  std::span<const ImTextureID> sprite_textures_;

  // Demo window
  bool show_demo_window_{false};
};

} // namespace sega

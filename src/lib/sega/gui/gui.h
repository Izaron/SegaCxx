#pragma once
#include "SDL_video.h"
#include "lib/sega/executor/executor.h"
#include "lib/sega/video/colors.h"
#include "lib/sega/video/tilemap.h"
#include <array>
#include <cstdint>
#include <functional>

namespace sega {

class Gui {
public:
  Gui(Executor& executor);
  bool setup();
  void loop();
  ~Gui();

private:
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

  // Cram window
  void add_colors_window();

  // Tilemap window
  void add_tilemap_window();

private:
  Executor& executor_;
  SDL_Window* window_{};
  SDL_GLContext gl_context_{};

  // Game window
  bool show_game_window_{true};

  // Execution window
  bool show_execution_window_{true};
  std::array<char, 6> until_address_{};
  std::function<bool()> condition_;
  uint32_t executed_count_{};

  // Colors window
  bool show_colors_window_{false};
  Colors colors_;

  // Tilemap window
  bool show_tilemap_window_{false};
  int tilemap_scale_{1};
  int tilemap_palette_{};
  Tilemap tilemap_;

  // Demo window
  bool show_demo_window_{false};
};

} // namespace sega

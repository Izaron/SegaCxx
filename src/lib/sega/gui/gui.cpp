#include "gui.h"
#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_opengl.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "fmt/format.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "lib/common/memory/types.h"
#include "lib/sega/executor/executor.h"
#include "lib/sega/rom_loader/rom_loader.h"
#include "lib/sega/video/colors.h"
#include "magic_enum/magic_enum.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ios>
#include <locale>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

// reference: https://github.com/ocornut/imgui/blob/master/examples/example_sdl2_opengl3/main.cpp

namespace sega {

namespace {

constexpr auto kFont = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
constexpr auto kClearColor = ImVec4{0.45, 0.55, 0.60, 1};
constexpr auto kRegisterColor = ImVec4{1, 0, 0, 1};    // red
constexpr auto kBytesColor = ImVec4{1, 1, 0, 1};       // yellow
constexpr auto kSizeColor = ImVec4{1, 1, 0, 1};        // yellow
constexpr auto kDescriptionColor = ImVec4{1, 0, 1, 1}; // pink

std::string make_title(const Metadata& metadata) {
  std::stringstream ss;
  const auto& title = metadata.domestic_title;
  for (size_t i = 0; i < title.size(); ++i) {
    if (i == 0 || !(title[i - 1] == ' ' && title[i] == ' ')) {
      ss << title[i];
    }
  }
  return ss.str();
}

} // namespace

Gui::Gui(Executor& executor)
    : executor_{executor}, tilemap_{executor_.vdp_device()}, sprite_table_{executor_.vdp_device(), colors_} {
  std::locale::global(std::locale("en_US.utf8"));
}

bool Gui::setup() {
  // setup SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
    spdlog::error("SDL_Init() error: {}", SDL_GetError());
    return false;
  }

  // setup GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // From 2.0.18: Enable native IME.
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  window_ = SDL_CreateWindow(make_title(executor_.metadata()).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             1280, 720, window_flags);
  if (window_ == nullptr) {
    spdlog::error("SDL_CreateWindow() error: {}", SDL_GetError());
    return false;
  }

  gl_context_ = SDL_GL_CreateContext(window_);
  SDL_GL_MakeCurrent(window_, gl_context_);
  SDL_GL_SetSwapInterval(1); // enable vsync

  // setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enable keyboard controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // enable gamepad controls

  // setup Dear ImGui style
  ImGui::StyleColorsDark();

  // setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // setup font
  io.Fonts->AddFontFromFileTTF(kFont, 18.0f);

  return true;
}

void Gui::loop() {
  bool done = false;
  while (!done) {
    // poll events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) {
        done = true;
      }
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window_)) {
        done = true;
      }
      if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(10);
      }
    }

    // execute instructions while condition is set or there is a VBlank
    while (condition_ && !condition_()) {
      const auto result = executor_.execute_current_instruction();
      ++executed_count_;
      if (!result.has_value()) {
        spdlog::error("current instruction error kind: {} what: {}", magic_enum::enum_name(result.error().kind()),
                      result.error().what());
      }
      if (result.value() == Executor::Result::VblankInterrupt) {
        break;
      }
    }
    if (condition_ && condition_()) {
      condition_ = nullptr;
    }

    // update core objects
    colors_.update(executor_.vdp_device().cram_data());

    // render frame
    // start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // add windows
    add_main_window();
    if (show_game_window_) {
      add_game_window();
    }
    if (show_execution_window_) {
      add_execution_window();
    }
    if (show_colors_window_) {
      add_colors_window();
    }
    if (show_tilemap_window_) {
      add_tilemap_window();
    }
    if (show_sprite_table_window_) {
      add_sprite_table_window();
    }
    if (show_demo_window_) {
      ImGui::ShowDemoWindow(&show_demo_window_);
    }

    // rendering
    ImGui::Render();
    auto& io = ImGui::GetIO();
    glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    glClearColor(kClearColor.x * kClearColor.w, kClearColor.y * kClearColor.w, kClearColor.z * kClearColor.w,
                 kClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
  }
}

Gui::~Gui() {
  // cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context_);
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

void Gui::add_main_window() {
  ImGui::Begin("Main", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Blast Processing!");
  ImGui::SeparatorText("Windows");
  ImGui::Checkbox("Game Window", &show_game_window_);
  ImGui::Checkbox("Execution Window", &show_execution_window_);
  ImGui::Checkbox("Colors Window", &show_colors_window_);
  ImGui::Checkbox("Tilemap Window", &show_tilemap_window_);
  ImGui::Checkbox("Sprite Table Window", &show_sprite_table_window_);
  ImGui::Checkbox("Demo Window", &show_demo_window_);
  if (ImGui::Button("Save Dump")) {
    executor_.save_dump_to_file("dump.bin");
  }

  auto& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

  ImGui::Separator();
  if (ImGui::TreeNode("ROM metadata")) {
    const auto& metadata = executor_.metadata();
    static constexpr auto kColor = ImVec4(1, 1, 0, 1);
#define ADD_TEXT(text, value)                                                                                          \
  ImGui::Text(text ":");                                                                                               \
  ImGui::SameLine();                                                                                                   \
  ImGui::TextColored(kColor, "%.*s", static_cast<int>((value).size()), (value).data());
    ADD_TEXT("System Type", metadata.system_type);
    ADD_TEXT("Copyright", metadata.copyright);
    ADD_TEXT("Domestic Title", metadata.domestic_title);
    ADD_TEXT("Overseas Title", metadata.overseas_title);
    ADD_TEXT("Serial Number", metadata.serial_number);
    ImGui::Text("Checksum:");
    ImGui::SameLine();
    ImGui::TextColored(kColor, "%02X", metadata.checksum.get());
    ADD_TEXT("Device Support", metadata.device_support);
    ImGui::Text("ROM Address:");
    ImGui::SameLine();
    ImGui::TextColored(kColor, "[%06X, %06X]", metadata.rom_address.begin.get(), metadata.rom_address.end.get());
    ImGui::Text("RAM Address:");
    ImGui::SameLine();
    ImGui::TextColored(kColor, "[%06X, %06X]", metadata.ram_address.begin.get(), metadata.ram_address.end.get());
    ADD_TEXT("Extra Memory", metadata.extra_memory);
    ADD_TEXT("Modem Support", metadata.modem_support);
    ADD_TEXT("Region Support", metadata.region_support);
#undef ADD_TEXT
    ImGui::TreePop();
  }

  ImGui::End();
}

void Gui::add_game_window() {}

void Gui::add_execution_window() {
  ImGui::Begin("Execution", &show_execution_window_, ImGuiWindowFlags_AlwaysAutoResize);
  add_execution_window_statistics();
  add_execution_window_instruction_info();
  add_execution_window_commands();
  add_execution_window_registers();
  ImGui::End();
}

void Gui::add_execution_window_statistics() {
  ImGui::SeparatorText("Statistics");
  ImGui::Text("Status: %s", condition_ ? "Running" : "Stopped");
  ImGui::Text("Executed Instructions: %s", fmt::format("{:L}", executed_count_).c_str());
  if (condition_) {
    auto& io = ImGui::GetIO();
    ImGui::Text("Performance: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  } else {
    ImGui::Text("Performance: <STOPPED>");
  }
}

void Gui::add_execution_window_instruction_info() {
  const auto instruction_info = executor_.current_instruction_info();
  ImGui::SeparatorText("Current Instruction");
  ImGui::Text("Program Counter =");
  ImGui::SameLine();
  ImGui::TextColored(kRegisterColor, "%08X", instruction_info.pc);
  ImGui::Text("Bytes =");
  ImGui::SameLine();
  ImGui::TextColored(kBytesColor, "%s", fmt::format("{}", instruction_info.bytes).c_str());
  ImGui::Text("Type =");
  ImGui::SameLine();
  ImGui::TextColored(kDescriptionColor, "%s", instruction_info.description.c_str());
}

void Gui::add_execution_window_commands() {
  bool has_condition = condition_ != nullptr;
  const auto& registers = executor_.registers();
  ImGui::SeparatorText("Commands");
  if (ImGui::Button("Run Current Instruction")) {
    condition_ = [cnt = 0] mutable { return cnt++ > 0; };
  }

  ImGui::Separator();
  if (ImGui::Button("Run Until Next Instruction")) {
    // TODO: cache it?
    const auto instruction_info = executor_.current_instruction_info();
    condition_ = [target_pc = instruction_info.pc + instruction_info.bytes.size(), &registers] {
      return registers.pc == target_pc;
    };
  }

  ImGui::Separator();
  if (ImGui::Button("Run Until Next VBLANK")) {
    // also check situation when already on vblank position
    const auto vblank_pc = executor_.vector_table().vblank_pc.get();
    condition_ = [cnt = registers.pc == vblank_pc ? 2 : 1, &registers, vblank_pc] mutable {
      if (registers.pc == vblank_pc) {
        --cnt;
      }
      return cnt <= 0;
    };
  }

  ImGui::Separator();
  if (ImGui::Button("Run Until Address")) {
    // not beautiful parsing of a hex value...
    std::stringstream ss;
    ss << std::hex << until_address_.data();
    AddressType target_pc;
    ss >> target_pc;
    condition_ = [&registers, target_pc] mutable { return registers.pc == target_pc; };
  }
  ImGui::InputText("Address", until_address_.data(), until_address_.size(),
                   ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

  ImGui::Separator();
  if (ImGui::Button("Run Forever")) {
    condition_ = [] { return false; };
  }

  ImGui::Separator();
  if (ImGui::Button("Pause")) {
    condition_ = nullptr;
  }

  // should reset interrupt time if condition has been jus tset
  if (!has_condition && condition_) {
    executor_.reset_interrupt_time();
  }
}

void Gui::add_execution_window_registers() {
  ImGui::SeparatorText("Registers");
  const auto& registers = executor_.registers();

#define ADD_REGISTER_WITH_SIZE(size, value, ...)                                                                       \
  ImGui::Text(__VA_ARGS__);                                                                                            \
  ImGui::SameLine();                                                                                                   \
  ImGui::TextColored(kRegisterColor, "%0 " size "X", value);
#define ADD_REGISTER(value, ...) ADD_REGISTER_WITH_SIZE("8", value, __VA_ARGS__)
  for (size_t i = 0; i < 7; ++i) {
    ADD_REGISTER(registers.d[i], "D%zu =", i)
    ImGui::SameLine();
    ADD_REGISTER(registers.a[i], "A%zu =", i)
  }
  ADD_REGISTER(registers.d[7], "D7 =")
  ADD_REGISTER(registers.usp, "USP =")
  ADD_REGISTER(registers.ssp, "SSP =")
  ADD_REGISTER(registers.pc, "PC =")

  // show status register
  ADD_REGISTER_WITH_SIZE("4", std::bit_cast<uint16_t>(registers.sr), "SR =")
  if (ImGui::TreeNode("Status Register")) {
#define ADD_STATUS_REGISTER(size, value, short_name, long_name)                                                        \
  ADD_REGISTER_WITH_SIZE(size, registers.sr.value, short_name " =")                                                    \
  ImGui::SameLine();                                                                                                   \
  ImGui::Text("[" long_name "]");
    ADD_STATUS_REGISTER("2", trace, "T", "Trace")
    ADD_STATUS_REGISTER("1", supervisor, "S", "Supervisor")
    ADD_STATUS_REGISTER("1", supervisor, "M", "Master Switch")
    ADD_STATUS_REGISTER("2", interrupt_mask, "I", "Interrupt Mask")
    ADD_STATUS_REGISTER("1", negative, "N", "Negative")
    ADD_STATUS_REGISTER("1", zero, "Z", "Zero")
    ADD_STATUS_REGISTER("1", overflow, "O", "Overflow")
    ADD_STATUS_REGISTER("1", carry, "C", "Carry")
    ImGui::TreePop();
  }
}

void Gui::add_colors_window() {
  ImGui::Begin("Colors", &show_colors_window_, ImGuiWindowFlags_AlwaysAutoResize);

  for (size_t palette_idx = 0; palette_idx < 4; ++palette_idx) {
    for (size_t color_idx = 0; color_idx < 16; ++color_idx) {
      if (color_idx > 0) {
        ImGui::SameLine();
      }
      const auto& color = colors_.color(palette_idx, color_idx);
      const auto tooltip = fmt::format("Palette {}, Color {}", palette_idx, color_idx);
      ImGui::ColorButton(tooltip.c_str(), color, ImGuiColorEditFlags_AlphaPreview, ImVec2{32, 32});
    }
    ImGui::SameLine();
    ImGui::Checkbox(fmt::format("Enabled##{}", palette_idx).c_str(), &colors_.palette_enabled(palette_idx));
  }
  ImGui::End();
}

void Gui::add_tilemap_window() {
  ImGui::Begin("Tilemap", &show_tilemap_window_, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Tilemap Size =");
  ImGui::SameLine();
  ImGui::TextColored(kSizeColor, "%dx%d", tilemap_.width(), tilemap_.height());

  for (int i = 0; i < 4; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }
    ImGui::RadioButton(fmt::format("Palette #{}", i).c_str(), &tilemap_palette_, i);
  }

  ImGui::SliderInt("Scale##Tilemap", &tilemap_scale_, /*v_min=*/1, /*v_max=*/5);

  const auto scale = 8 * static_cast<float>(tilemap_scale_);
  const auto width = scale * static_cast<float>(tilemap_.width());
  const auto height = scale * static_cast<float>(tilemap_.height());
  ImGui::Image(tilemap_.draw(colors_.palette(tilemap_palette_)), ImVec2(width, height),
               /*uv0=*/ImVec2(0, 0), /*uv1=*/ImVec2(1, 1), /*tint_col=*/ImVec4(1, 1, 1, 1),
               /*border_col=*/ImVec4(1, 1, 1, 1));
  ImGui::End();
}

void Gui::add_sprite_table_window() {
  ImGui::Begin("Sprite Table", &show_sprite_table_window_);

  ImGui::Checkbox("Auto Update##Sprite Table", &sprite_table_auto_update_);

  ImGui::SliderInt("Scale##Sprite Table", &sprite_scale_, /*v_min=*/1, /*v_max=*/8);

  if (ImGui::Button("Draw Sprites") || sprite_table_auto_update_) {
    sprites_ = sprite_table_.read_sprites();
    sprite_textures_ = sprite_table_.draw_sprites();
  }

  static constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable("sprite_table", 2, kFlags)) {
    ImGui::TableSetupColumn("Description");
    ImGui::TableSetupColumn("Image");
    ImGui::TableHeadersRow();
    for (const auto& [sprite, texture] : std::views::zip(sprites_, sprite_textures_)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Coordinate =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%dx%d", sprite.x_coord, sprite.y_coord);
      ImGui::Text("Size in tiles =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%dx%d", sprite.width, sprite.height);
      ImGui::Text("Tile ID =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%d", sprite.tile_id);
      ImGui::Text("Palette =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%d", sprite.palette);
      ImGui::Text("Priority =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%d", sprite.priority);
      ImGui::Text("Flip H =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%d", sprite.flip_horizontally);
      ImGui::Text("Flip V =");
      ImGui::SameLine();
      ImGui::TextColored(kSizeColor, "%d", sprite.flip_vertically);

      ImGui::TableNextColumn();
      const auto scale = 8 * static_cast<float>(sprite_scale_);
      const auto width = scale * static_cast<float>(sprite.width);
      const auto height = scale * static_cast<float>(sprite.height);
      ImGui::Image(texture, ImVec2(width, height),
                   /*uv0=*/ImVec2(0, 0), /*uv1=*/ImVec2(1, 1), /*tint_col=*/ImVec4(1, 1, 1, 1),
                   /*border_col=*/ImVec4(1, 1, 1, 1));
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

} // namespace sega

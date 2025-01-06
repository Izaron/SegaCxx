#include <cctype>
#include <glad/gl.h>

#include "GLFW/glfw3.h"
#include "fmt/format.h"
#include "gui.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "lib/common/memory/types.h"
#include "lib/sega/executor/executor.h"
#include "lib/sega/memory/controller_device.h"
#include "lib/sega/rom_loader/rom_loader.h"
#include "lib/sega/shader/shader.h"
#include "lib/sega/video/colors.h"
#include "lib/sega/video/constants.h"
#include "lib/sega/video/plane.h"
#include "magic_enum/magic_enum.hpp"
#include <GL/gl.h>
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
#include <string_view>
#include <utility>

// reference: https://github.com/ocornut/imgui/blob/master/examples/example_sdl2_opengl3/main.cpp

namespace sega {

namespace {

constexpr auto kFont = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
constexpr auto kClearColor = ImVec4{0.45, 0.55, 0.60, 1};
constexpr auto kRegisterColor = ImVec4{1, 0, 0, 1};    // red
constexpr auto kBytesColor = ImVec4{1, 1, 0, 1};       // yellow
constexpr auto kSizeColor = ImVec4{1, 1, 0, 1};        // yellow
constexpr auto kDescriptionColor = ImVec4{1, 0, 1, 1}; // pink

void glfw_error_callback(int error, const char* description) {
  spdlog::error("GLFW error code: {} description: {}", error, description);
}

std::string make_title(const Metadata& metadata) {
  std::stringstream ss;
  const auto& title = std::isalnum(metadata.domestic_title[0]) ? metadata.domestic_title : metadata.overseas_title;
  for (size_t i = 0; i < title.size(); ++i) {
    if (i == 0 || !(title[i - 1] == ' ' && title[i] == ' ')) {
      ss << title[i];
    }
  }
  return ss.str();
}

} // namespace

Gui::Gui(Executor& executor)
    : executor_{executor}, video_{executor_.vdp_device()}, tilemap_{executor_.vdp_device()},
      planes_{Plane{executor_.vdp_device(), PlaneType::PlaneA}, Plane{executor_.vdp_device(), PlaneType::PlaneB},
              Plane{executor_.vdp_device(), PlaneType::Window}} {
  std::locale::global(std::locale("en_US.utf8"));
}

Gui::~Gui() {
  // cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window_);
  glfwTerminate();
}

bool Gui::setup() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return false;
  }

  // setup GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // setup
  window_ = glfwCreateWindow(1280, 720, make_title(executor_.metadata()).c_str(), nullptr, nullptr);
  if (window_ == nullptr) {
    return false;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(0);
  if (!gladLoadGL(glfwGetProcAddress)) {
    return false;
  }

  // setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enable keyboard controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // enable gamepad controls

  // setup Dear ImGui style
  ImGui::StyleColorsDark();

  // setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // setup font
  io.Fonts->AddFontFromFileTTF(kFont, 18.0f);

  // build shader programs
  shader_.build_programs();

  return true;
}

void Gui::loop() {
  while (poll_events()) {
    update_controller();
    execute();
    video_.update();
    render();
  }
}

void Gui::execute() {
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
}

void Gui::render() {
  // start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
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
  for (size_t i = 0; i < kPlaneTypes; ++i) {
    if (show_plane_window_[i]) {
      add_plane_window(static_cast<PlaneType>(i));
    }
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
  glfwSwapBuffers(window_);
}

bool Gui::poll_events() {
  if (glfwWindowShouldClose(window_)) {
    return false;
  }
  glfwPollEvents();
  if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0) {
    ImGui_ImplGlfw_Sleep(10);
  }
  return true;
}

void Gui::update_controller() {
  static constexpr std::array kMap = {
      // keyboard keys
      std::make_pair(ImGuiKey_Enter, ControllerDevice::Button::Start),

      std::make_pair(ImGuiKey_LeftArrow, ControllerDevice::Button::Left),
      std::make_pair(ImGuiKey_RightArrow, ControllerDevice::Button::Right),
      std::make_pair(ImGuiKey_UpArrow, ControllerDevice::Button::Up),
      std::make_pair(ImGuiKey_DownArrow, ControllerDevice::Button::Down),

      std::make_pair(ImGuiKey_A, ControllerDevice::Button::A),
      std::make_pair(ImGuiKey_S, ControllerDevice::Button::B),
      std::make_pair(ImGuiKey_D, ControllerDevice::Button::C),

      // Retroflag joystick buttons
      std::make_pair(ImGuiKey_GamepadStart, ControllerDevice::Button::Start),

      std::make_pair(ImGuiKey_GamepadDpadLeft, ControllerDevice::Button::Left),
      std::make_pair(ImGuiKey_GamepadDpadRight, ControllerDevice::Button::Right),
      std::make_pair(ImGuiKey_GamepadDpadUp, ControllerDevice::Button::Up),
      std::make_pair(ImGuiKey_GamepadDpadDown, ControllerDevice::Button::Down),

      std::make_pair(ImGuiKey_GamepadFaceDown, ControllerDevice::Button::A),
      std::make_pair(ImGuiKey_GamepadFaceRight, ControllerDevice::Button::B),
      std::make_pair(ImGuiKey_GamepadR2, ControllerDevice::Button::C),
  };

  auto& controller = executor_.controller_device();
  for (const auto& [key, button] : kMap) {
    if (ImGui::IsKeyPressed(key, /*repeat=*/false)) {
      controller.set_button(button, true);
    } else if (ImGui::IsKeyReleased(key)) {
      controller.set_button(button, false);
    }
  }
}

void Gui::add_main_window() {
  ImGui::Begin("Main", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
  ImGui::Text("Blast Processing!");
  ImGui::SeparatorText("Windows");
  ImGui::Checkbox("Game Window", &show_game_window_);
  ImGui::Checkbox("Execution Window", &show_execution_window_);
  ImGui::Checkbox("Colors Window", &show_colors_window_);
  ImGui::Checkbox("Tilemap Window", &show_tilemap_window_);
  ImGui::Checkbox("\"Plane A\" Plane Window", &show_plane_window_[0]);
  ImGui::Checkbox("\"Plane B\" Plane Window", &show_plane_window_[1]);
  ImGui::Checkbox("\"Window\" Plane Window", &show_plane_window_[2]);
  ImGui::Checkbox("Sprite Table Window", &show_sprite_table_window_);
  // ImGui::Checkbox("Demo Window", &show_demo_window_);
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

void Gui::add_game_window() {
  ImGui::Begin("Game", &show_game_window_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);

  // window size info
  ImGui::Text("Window Size =");
  ImGui::SameLine();
  ImGui::TextColored(kSizeColor, "%dx%d", video_.width() * kTileDimension, video_.height() * kTileDimension);
  ImGui::SameLine();
  ImGui::Text("pixels");

  // shader selection
  if (ImGui::Button("Select Shader")) {
    ImGui::OpenPopup("shader_popup");
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(magic_enum::enum_name(current_shader_type_).data());
  if (ImGui::BeginPopup("shader_popup")) {
    ImGui::SeparatorText("Shader Type");
    for (const auto value : magic_enum::enum_values<ShaderType>()) {
      if (ImGui::Selectable(magic_enum::enum_name(value).data())) {
        current_shader_type_ = value;
      }
    }
    ImGui::EndPopup();
  }

  // game speed selection
  const auto game_speed_str = std::invoke([game_speed = game_speed_] {
    switch (game_speed) {
    case GameSpeed::x0p25:
      return "x0.25";
    case GameSpeed::x0p50:
      return "x0.5";
    case GameSpeed::x1p00:
      return "x1";
    case GameSpeed::x1p50:
      return "x1.5";
    case GameSpeed::x2p00:
      return "x2";
    }
  });
  const auto game_speed_value = std::invoke([game_speed = game_speed_] {
    switch (game_speed) {
    case GameSpeed::x0p25:
      return 0.25;
    case GameSpeed::x0p50:
      return 0.5;
    case GameSpeed::x1p00:
      return 1.0;
    case GameSpeed::x1p50:
      return 1.5;
    case GameSpeed::x2p00:
      return 2.0;
    }
  });

  ImGui::SliderInt("Game Speed", reinterpret_cast<int*>(&game_speed_), 0, magic_enum::enum_count<GameSpeed>() - 1,
                   game_speed_str);
  executor_.set_game_speed(game_speed_value);

  // scale selection
  ImGui::SliderInt("Scale##Game", &game_scale_, /*v_min=*/1, /*v_max=*/8);

  // draw game to a texture
  const auto texture = video_.draw();
  const auto scale = kTileDimension * static_cast<float>(game_scale_);
  const auto width = scale * static_cast<float>(video_.width());
  const auto height = scale * static_cast<float>(video_.height());

  // setup shader
  shader_program_ = shader_.get_program(current_shader_type_);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddCallback(
      [](const ImDrawList*, const ImDrawCmd* draw_cmd) {
        ImDrawData* draw_data = ImGui::GetDrawData();
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

        const float ortho_projection[4][4] = {
            {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.0f, 1.0f},
        };

        const auto program = *reinterpret_cast<GLuint*>(draw_cmd->UserCallbackData);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "ProjMtx"), 1, GL_FALSE, &ortho_projection[0][0]);
      },
      reinterpret_cast<void*>(&shader_program_));

  ImGui::Image(texture, ImVec2(width, height),
               /*uv0=*/ImVec2(0, 0), /*uv1=*/ImVec2(1, 1), /*tint_col=*/ImVec4(1, 1, 1, 1),
               /*border_col=*/ImVec4(1, 1, 1, 1));
  ImGui::End();
}

void Gui::add_execution_window() {
  ImGui::Begin("Execution", &show_execution_window_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
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
  ImGui::Begin("Colors", &show_colors_window_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);

  for (size_t palette_idx = 0; palette_idx < 4; ++palette_idx) {
    for (size_t color_idx = 0; color_idx < 16; ++color_idx) {
      if (color_idx > 0) {
        ImGui::SameLine();
      }
      const auto& color = video_.colors().color(palette_idx, color_idx);
      const auto tooltip = fmt::format("Palette {}, Color {}", palette_idx, color_idx);
      ImVec4 gui_color{static_cast<float>(color.red) / 255, static_cast<float>(color.green) / 255,
                       static_cast<float>(color.blue) / 255, (color_idx == 0) ? 0.75f : 1.0f};
      ImGui::ColorButton(tooltip.c_str(), gui_color, ImGuiColorEditFlags_AlphaPreview, ImVec2{32, 32});
    }
  }
  ImGui::End();
}

void Gui::add_tilemap_window() {
  ImGui::Begin("Tilemap", &show_tilemap_window_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);

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
  ImGui::Image(tilemap_.draw(video_.colors().palette(tilemap_palette_)), ImVec2(width, height),
               /*uv0=*/ImVec2(0, 0), /*uv1=*/ImVec2(1, 1), /*tint_col=*/ImVec4(1, 1, 1, 1),
               /*border_col=*/ImVec4(1, 1, 1, 1));
  ImGui::End();
}

void Gui::add_plane_window(PlaneType plane_type) {
  const size_t plane_idx = static_cast<size_t>(plane_type);
  static constexpr std::array<std::string_view, kPlaneTypes> kNames = {
      "Plane A plane",
      "Plane B plane",
      "Window plane",
  };
  static constexpr std::array<std::string_view, kPlaneTypes> kScaleLabel = {
      "Scale##PlaneA",
      "Scale##PlaneB",
      "Scale##Window",
  };

  ImGui::Begin(kNames[plane_idx].data(), &show_plane_window_[plane_idx],
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
  ImGui::Text("Tilemap Size =");
  ImGui::SameLine();
  ImGui::TextColored(kSizeColor, "%dx%d", planes_[plane_idx].width(), planes_[plane_idx].height());

  ImGui::SliderInt(kScaleLabel[plane_idx].data(), &plane_scale_[plane_idx], /*v_min=*/1, /*v_max=*/5);

  const auto scale = 8 * static_cast<float>(plane_scale_[plane_idx]);
  const auto width = scale * static_cast<float>(planes_[plane_idx].width());
  const auto height = scale * static_cast<float>(planes_[plane_idx].height());
  ImGui::Image(planes_[plane_idx].draw(video_.colors()), ImVec2(width, height),
               /*uv0=*/ImVec2(0, 0), /*uv1=*/ImVec2(1, 1), /*tint_col=*/ImVec4(1, 1, 1, 1),
               /*border_col=*/ImVec4(1, 1, 1, 1));
  ImGui::End();
}

void Gui::add_sprite_table_window() {
  ImGui::Begin("Sprite Table", &show_sprite_table_window_, ImGuiWindowFlags_NoNav);

  ImGui::Checkbox("Auto Update##Sprite Table", &sprite_table_auto_update_);

  ImGui::SliderInt("Scale##Sprite Table", &sprite_scale_, /*v_min=*/1, /*v_max=*/8);

  if (ImGui::Button("Draw Sprites") || sprite_table_auto_update_) {
    sprites_ = video_.sprite_table().read_sprites();
    sprite_textures_ = video_.sprite_table().draw_sprites();
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

#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include "lib/common/util/passkey.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace sega {

class VdpDevice : public Device {
public:
  static constexpr AddressType kBegin = 0xC00000;
  static constexpr AddressType kEnd = 0xC0000E;

  enum class HorizontalScrollMode : uint8_t {
    FullScroll = 0b00,
    Invalid = 0b01,
    ScrollEveryTile = 0b10,
    ScrollEveryLine = 0b11,
  };

  enum class VerticalScrollMode : uint8_t {
    FullScroll = 0,
    ScrollEveryTwoTiles = 1,
  };

  enum class WindowSplitMode {
    X,
    Y,
  };

public:
  VdpDevice(Device& bus_device);

  // data from registers
  bool vblank_interrupt_enabled() const {
    return vblank_interrupt_enabled_;
  }

  uint8_t tile_width() const {
    return width_;
  }
  uint8_t tile_height() const {
    return height_;
  }

  uint8_t plane_width() const {
    return plane_width_;
  }
  uint8_t plane_height() const {
    return plane_height_;
  }

  HorizontalScrollMode horizontal_scroll_mode() const {
    return horizontal_scroll_mode_;
  }
  VerticalScrollMode vertical_scroll_mode() const {
    return vertical_scroll_mode_;
  }
  Word hscroll_table_address() const {
    return hscroll_table_address_;
  }

  Word plane_a_table_address() const {
    return plane_a_table_address_;
  }
  Word plane_b_table_address() const {
    return plane_b_table_address_;
  }

  Word window_table_address() const {
    return window_table_address_;
  }
  uint16_t window_x_split() const {
    return window_x_split_;
  }
  uint16_t window_y_split() const {
    return window_y_split_;
  }
  bool window_display_to_the_right() const {
    return window_display_to_the_right_;
  }
  bool window_display_below() const {
    return window_display_below_;
  }

  Word sprite_table_address() const {
    return sprite_table_address_;
  }

  uint8_t background_color_palette() const {
    return background_color_palette_;
  }
  uint8_t background_color_index() const {
    return background_color_index_;
  }

  // video RAM data
  DataView vram_data() const {
    return vram_data_;
  }
  DataView vsram_data() const {
    return vsram_data_;
  }
  DataView cram_data() const {
    return cram_data_;
  }

  // dump or apply whole VDP state
  std::vector<Byte> dump_state(Passkey<class StateDump>) const;
  void apply_state(Passkey<StateDump>, DataView state);

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;
  std::optional<Error> write(AddressType addr, DataView data) override;

  [[nodiscard]] std::optional<Error> process_vdp_control(Word command);
  [[nodiscard]] std::optional<Error> process_vdp_data(Word command);

  [[nodiscard]] std::optional<Error> process_vdp_register(Word command);
  void process_mode1_set(Byte value);
  void process_mode2_set(Byte value);
  void process_plane_a_table_address(Byte value);
  void process_window_table_address(Byte value);
  void process_plane_b_table_address(Byte value);
  void process_sprite_table_address(Byte value);
  void process_background_color(Byte value);
  void process_hblank_interrupt_rate(Byte value);
  void process_mode3_set(Byte value);
  void process_mode4_set(Byte value);
  void process_hscroll_table_address(Byte value);
  void process_auto_increment(Byte value);
  void process_plane_size(Byte value);
  void process_window_x_division(Byte value);
  void process_window_y_division(Byte value);
  void process_dma_length_low(Byte value);
  void process_dma_length_high(Byte value);
  void process_dma_source_low(Byte value);
  void process_dma_source_middle(Byte value);
  void process_dma_source_high(Byte value);

  Word read_status_register();

  std::vector<Byte>& ram_data();

private:
  enum class DmaType : uint8_t {
    MemoryToVram,
    VramFill,
    VramCopy,
  };

  enum class RamKind : uint8_t {
    Vram,
    Vsram,
    Cram,
  };

private:
  // data from registers
  bool vblank_interrupt_enabled_{};

  bool allow_dma_{};
  Long dma_length_words_{}; // warning - size in words, not in bytes
  Long dma_source_words_{}; // warning - size in words, not in bytes
  DmaType dma_type_{DmaType::MemoryToVram};
  Byte auto_increment_{};

  uint8_t width_{};
  uint8_t height_{};

  uint8_t plane_width_{};
  uint8_t plane_height_{};

  HorizontalScrollMode horizontal_scroll_mode_{};
  VerticalScrollMode vertical_scroll_mode_{};
  Word hscroll_table_address_{};

  Word plane_a_table_address_{};
  Word plane_b_table_address_{};

  Word window_table_address_{};
  uint16_t window_x_split_{};
  uint16_t window_y_split_{};
  bool window_display_to_the_right_{};
  bool window_display_below_{};

  Word sprite_table_address_{};

  uint8_t background_color_palette_{};
  uint8_t background_color_index_{};

  // video RAM address
  std::optional<Word> first_half_;
  bool use_dma_{};
  RamKind ram_kind_{RamKind::Vram};
  Word ram_address_{};

  // registers data
  std::vector<Byte> registers_;

  // video RAMs data
  std::vector<Byte> vram_data_;
  std::vector<Byte> vsram_data_;
  std::vector<Byte> cram_data_;

  // memory bus device
  Device& bus_device_;
};

} // namespace sega

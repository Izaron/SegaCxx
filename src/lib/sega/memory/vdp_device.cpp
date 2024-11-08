#include "vdp_device.h"
#include "fmt/format.h"
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include "magic_enum/magic_enum.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <vector>

// reference: https://plutiedev.com/writing-video
// reference: https://plutiedev.com/dma-transfer
// reference: https://wiki.megadrive.org/index.php?title=VDP_Ports

namespace sega {

namespace {

constexpr AddressType kVdpData1 = 0xC00000;
constexpr AddressType kVdpData2 = 0xC00002;
constexpr AddressType kVdpCtrl1 = 0xC00004;
constexpr AddressType kVdpCtrl2 = 0xC00006;

constexpr AddressType kVramSize = 65536;
constexpr AddressType kVsramSize = 80;
constexpr AddressType kCramSize = 128;

constexpr Long kVramAddrCmd = 0x40000000;
constexpr Long kVsramAddrCmd = 0x40000010;
constexpr Long kCramAddrCmd = 0xC0000000;

constexpr Long kVramDmaCmd = 0x40000080;
constexpr Long kVsramDmaCmd = 0x40000090;
constexpr Long kCramDmaCmd = 0xC0000080;

constexpr AddressType kSpriteAddressScale = 0x200;
constexpr AddressType kHscrollAddressScale = 0x400;
constexpr AddressType kWindowAddressScale = 0x800;
constexpr AddressType kPlaneAddressScale = 0x2000;

struct StatusRegister {
  enum class Mode : uint8_t {
    NTSC = 0,
    PAL = 1,
  };

  enum class DmaStatus : uint8_t {
    NotBusy = 0,
    Busy = 1,
  };

  enum class HblankStatus : uint8_t {
    NotInHblank = 0,
    InHblank = 1,
  };

  enum class VblankStatus : uint8_t {
    NotInVblank = 0,
    InVblank = 1,
  };

  enum class FrameStatus : uint8_t {
    EvenFrame = 0,
    OddFrame = 1,
  };

  enum class CollisionStatus : uint8_t {
    NoCollision = 0,
    CollisionBetweenTwoSprites = 1,
  };

  enum class SpritesOverflowStatus : uint8_t {
    NoSpritesOverflow = 0,
    SpritesOverflow = 1,
  };

  enum class InterruptStatus : uint8_t {
    InterruptNotHappened = 0,
    InterruptHappened = 1,
  };

  enum class FifoFullStatus : uint8_t {
    FifoNotFull = 0,
    FifoFull = 1,
  };

  enum class FifoEmptyStatus : uint8_t {
    FifoNotEmpty = 0,
    FifoEmpty = 1,
  };

  Mode mode : 1;
  DmaStatus dma_status : 1;
  HblankStatus hblank_status : 1;
  VblankStatus vblank_status : 1;
  FrameStatus frame_status : 1;
  CollisionStatus collision_status : 1;
  SpritesOverflowStatus sprites_overflow_status : 1;
  InterruptStatus interrupt_status : 1;
  FifoFullStatus fifo_full_status : 1;
  FifoEmptyStatus fifo_empty_status : 1;
  bool : 6;
};
static_assert(sizeof(StatusRegister) == 2);

enum class VdpRegister : Byte {
  ModeSet1 = 0x80,
  ModeSet2 = 0x81,
  PlaneATableAddress = 0x82,
  WindowTableAddress = 0x83,
  PlaneBTableAddress = 0x84,
  SpriteTableAddress = 0x85,
  Unused86 = 0x86,
  BackgroundColor = 0x87,
  Unused88 = 0x88,
  Unused89 = 0x89,
  HblankInterruptRate = 0x8A,
  ModeSet3 = 0x8B,
  ModeSet4 = 0x8C,
  HscrollTableAddress = 0x8D,
  Unused8E = 0x8E,
  AutoIncrement = 0x8F,
  TilemapSize = 0x90,
  WindowXDivision = 0x91,
  WindowYDivision = 0x92,
  DmaLengthLow = 0x93,
  DmaLengthHigh = 0x94,
  DmaSourceLow = 0x95,
  DmaSourceMiddle = 0x96,
  DmaSourceHigh = 0x97,
};

struct Mode1 {
  bool : 1;
  bool freeze_hv_counter : 1;
  bool : 2;
  bool enable_hblank_interrupt : 1;
  bool blank_leftmost_column : 1;
  bool : 2;
};
static_assert(sizeof(Mode1) == 1);

struct Mode2 {
  enum class VerticalResolution : uint8_t {
    V28 = 0,
    V30 = 1,
  };

  bool : 3;
  VerticalResolution vertical_resolution : 1;
  bool allow_dma : 1;
  bool enable_vblank_interrupt : 1;
  bool enable_rendering : 1;
  bool : 1;
};
static_assert(sizeof(Mode2) == 1);

struct Mode3 {
  enum class HorizontalScrollMode : uint8_t {
    FullScroll = 0b00,
    ScrollEightLines = 0b01,
    ScrollEveryTile = 0b10,
    ScrollEveryLine = 0b11,
  };

  enum class VerticalScrollMode : uint8_t {
    FullScroll = 0,
    ScrollEveryTwoTiles = 1,
  };

  HorizontalScrollMode horizontal_scroll_mode : 2;
  VerticalScrollMode vertical_scroll_mode : 1;
  bool enable_external_interrupt : 1;
  bool : 4;
};
static_assert(sizeof(Mode3) == 1);

struct Mode4 {
  enum class HorizontalResolution : uint8_t {
    H32 = 0,
    H40 = 1,
  };

  enum class InterlacedMode : uint8_t {
    NoInterlacing = 0b00,
    Mode1 = 0b01,
    Mode2 = 0b11,
  };

  HorizontalResolution horizontal_resolution : 1;
  InterlacedMode interlaced_mode : 2;
  bool enable_shadow_highlight : 1;
  bool : 4;
};
static_assert(sizeof(Mode4) == 1);

struct PlaneATableAddress {
  bool : 3;
  uint8_t address : 3;
  bool : 2;
};
static_assert(sizeof(PlaneATableAddress) == 1);

struct WindowTableAddress {
  bool : 1;
  uint8_t address : 5;
  bool : 2;
};
static_assert(sizeof(WindowTableAddress) == 1);

struct PlaneBTableAddress {
  uint8_t address : 3;
  bool : 5;
};
static_assert(sizeof(PlaneBTableAddress) == 1);

struct SpriteTableAddress {
  uint8_t address : 7;
  bool : 1;
};
static_assert(sizeof(SpriteTableAddress) == 1);

struct BackgroundColor {
  uint8_t index : 4;
  uint8_t palette : 2;
  bool : 2;
};
static_assert(sizeof(BackgroundColor) == 1);

struct HscrollTableAddress {
  uint8_t address : 6;
  bool : 2;
};
static_assert(sizeof(HscrollTableAddress) == 1);

struct TilemapSize {
  enum class Size : uint8_t {
    S32 = 0b00,
    S64 = 0b01,
    S128 = 0b11,
  };

  Size horizontal_size : 2;
  bool : 2;
  Size vertical_size : 2;
  bool : 2;
};
static_assert(sizeof(TilemapSize) == 1);

struct WindowXDivision {
  uint8_t split_coordinate : 5;
  bool : 2;
  bool display_to_the_right : 1;
};
static_assert(sizeof(WindowXDivision) == 1);

struct WindowYDivision {
  uint8_t split_coordinate : 5;
  bool : 2;
  bool display_below : 1;
};
static_assert(sizeof(WindowYDivision) == 1);

struct DmaSourceHigh {
  enum class OperationType : uint8_t {
    DmaTransfer0 = 0b00,
    DmaTransfer1 = 0b01,
    VramFill = 0b10,
    VramCopy = 0b11,
  };

  uint8_t value : 6;
  OperationType operation_type : 2;
};
static_assert(sizeof(DmaSourceHigh) == 1);

} // namespace

VdpDevice::VdpDevice(Device& bus_device) : bus_device_{bus_device} {
  vram_data_.resize(kVramSize);
  vsram_data_.resize(kVsramSize);
  cram_data_.resize(kCramSize);
}

std::optional<Error> VdpDevice::read(AddressType addr, MutableDataView data) {
  if (data.size() % 2 != 0) {
    return Error{Error::UnalignedMemoryRead,
                 fmt::format("Unaligned VDP read address: {:06x} size: {:x}", addr, data.size())};
  }

  for (size_t i = 0; i < data.size(); i += 2) {
    switch (addr + i) {
    case kVdpData1:
    case kVdpData2: {
      // TODO: maybe check for bounds?
      const auto& ram = ram_data();
      data[i] = ram[ram_address_++];
      data[i + 1] = ram[ram_address_++];
      break;
    }
    case kVdpCtrl1:
    case kVdpCtrl2: {
      const Word status_register = read_status_register();
      data[i] = status_register >> 8;
      data[i + 1] = status_register & 0xFF;
      break;
    }
    default:
      return Error{Error::InvalidRead, fmt::format("Invalid VDP read address: {:06x} size: {}", addr, data.size())};
    }
  }

  return std::nullopt;
}

std::optional<Error> VdpDevice::write(AddressType addr, DataView data) {
  if (data.size() % 2 != 0) {
    return Error{Error::UnalignedMemoryWrite,
                 fmt::format("Unaligned VDP write addr: {:06x} size: {}", addr, data.size())};
  }

  for (size_t i = 0; i < data.size(); i += 2) {
    const Word word = (Word{data[i]} << 8) | data[i + 1];
    switch (addr + i) {
    case kVdpData1:
    case kVdpData2:
      if (auto err = process_vdp_data(word)) {
        return err;
      }
      break;
    case kVdpCtrl1:
    case kVdpCtrl2:
      if (auto err = process_vdp_control(word)) {
        return err;
      }
      break;
    default:
      return Error{Error::InvalidWrite, fmt::format("invalid VDP write address: {:06x} size: {}", addr, data.size())};
    }
  }

  return std::nullopt;
}

std::optional<Error> VdpDevice::process_vdp_control(Word command) {
  // registers are when the three higher bits are '100'
  if ((command & 0b1110'0000'0000'0000) == 0b1000'0000'0000'0000) {
    return process_vdp_register(command);
  }
  // set RAM address first half
  else if (!first_half_.has_value()) {
    first_half_ = command;
  }
  // set RAM address second half
  else {
    const Long value = (Long{*first_half_} << 16) | command;
    ram_address_ = ((value & 0x3FFF0000) >> 16) | ((value & 0x3) << 14);
    const auto cd0 = (value & (1 << 30)) >> 30;
    const auto cd1 = (value & (1 << 31)) >> 31;
    const auto cd2 = (value & (1 << 4)) >> 4;
    const auto cd3 = (value & (1 << 5)) >> 5;
    const auto cd4 = (value & (1 << 6)) >> 6;
    const auto cd5 = (value & (1 << 7)) >> 7;

    use_dma_ = cd5;
    // TODO: check for enabled DMA from Mode2

    const auto mask = (cd3 << 3) | (cd2 << 2) | (cd1 << 1) | cd0;
    switch (mask) {
    case 0b0001: // write
    case 0b0000: // read
      ram_kind_ = RamKind::Vram;
      break;
    case 0b0011: // write
    case 0b1000: // read
      ram_kind_ = RamKind::Cram;
      break;
    case 0b0101: // write
    case 0b0100: // read
      ram_kind_ = RamKind::Vsram;
      break;
    default:
      return Error{Error::InvalidWrite, fmt::format("Invalid RAM kind value: {:08x}", value)};
    }
    const bool is_write = (mask == 0b0001) || (mask == 0b0011) || (mask == 0b0101);

    spdlog::debug("set RAM address: {:04x} ram_kind: {} use_dma: {} is_write: {}", ram_address_,
                  magic_enum::enum_name(ram_kind_), use_dma_, is_write);

    if (use_dma_ && dma_type_ == DmaType::VramCopy) {
      return Error{Error::InvalidWrite, fmt::format("Unsupported DMA type yet: {:08x}", value)};
    }

    if (use_dma_ && dma_type_ == DmaType::MemoryToVram) {
      // perform DMA immediately
      const auto source_start = dma_source_words_ << 1;
      const auto len = dma_length_words_ << 1;
      spdlog::debug("perform memory to vram DMA source_start: {:06x} len: {:04x} dest: {:04x}", source_start, len,
                    ram_address_);

      auto& ram = ram_data();
      if (auto err = bus_device_.read(source_start, {ram.data() + ram_address_, len})) {
        return err;
      }
      use_dma_ = false;
    }

    first_half_.reset();
  }
  return std::nullopt;
}

std::optional<Error> VdpDevice::process_vdp_data(Word data) {
  if (use_dma_) {
    assert(dma_type_ == DmaType::VramFill);
  }

  if (use_dma_ && dma_type_ == DmaType::VramFill) {
    if (auto_increment_ != 1) {
      return Error{Error::InvalidWrite, fmt::format("Invalid auto increment, expected: 1 got: {:x}", auto_increment_)};
    }
    auto& ram = ram_data();
    size_t end_address_ = ram_address_ + (dma_length_words_ << 1);
    end_address_ = std::min(ram.size(), end_address_);
    spdlog::debug("fill ram_kind: {} data: {:04x} begin: {:06x} end: {:06x}", magic_enum::enum_name(ram_kind_), data,
                  ram_address_, end_address_);
    for (size_t i = ram_address_; i < end_address_; i += 2) {
      ram[i] = data >> 8;
      ram[i + 1] = data & 0xFF;
    }
    ram_address_ = end_address_;
    use_dma_ = false;
    return std::nullopt;
  }

  auto& ram = ram_data();
  ram[ram_address_++] = data >> 8;
  ram[ram_address_++] = data & 0xFF;
  ram_address_ += auto_increment_ - 2;
  return std::nullopt;
}

std::optional<Error> VdpDevice::process_vdp_register(Word data) {
  const Byte kind = data >> 8;
  const Byte value = data & 0xFF;
  switch (static_cast<VdpRegister>(kind)) {
  case VdpRegister::ModeSet1:
    process_mode1_set(value);
    break;
  case VdpRegister::ModeSet2:
    process_mode2_set(value);
    break;
  case VdpRegister::PlaneATableAddress:
    process_plane_a_table_address(value);
    break;
  case VdpRegister::WindowTableAddress:
    process_window_table_address(value);
    break;
  case VdpRegister::PlaneBTableAddress:
    process_plane_b_table_address(value);
    break;
  case VdpRegister::SpriteTableAddress:
    process_sprite_table_address(value);
    break;
  case VdpRegister::BackgroundColor:
    process_background_color(value);
    break;
  case VdpRegister::HblankInterruptRate:
    process_hblank_interrupt_rate(value);
    break;
  case VdpRegister::ModeSet3:
    process_mode3_set(value);
    break;
  case VdpRegister::ModeSet4:
    process_mode4_set(value);
    break;
  case VdpRegister::HscrollTableAddress:
    process_hscroll_table_address(value);
    break;
  case VdpRegister::AutoIncrement:
    process_auto_increment(value);
    break;
  case VdpRegister::TilemapSize:
    process_tilemap_size(value);
    break;
  case VdpRegister::WindowXDivision:
    process_window_x_division(value);
    break;
  case VdpRegister::WindowYDivision:
    process_window_y_division(value);
    break;
  case VdpRegister::DmaLengthLow:
    process_dma_length_low(value);
    break;
  case VdpRegister::DmaLengthHigh:
    process_dma_length_high(value);
    break;
  case VdpRegister::DmaSourceLow:
    process_dma_source_low(value);
    break;
  case VdpRegister::DmaSourceMiddle:
    process_dma_source_middle(value);
    break;
  case VdpRegister::DmaSourceHigh:
    process_dma_source_high(value);
    break;
  case VdpRegister::Unused86:
  case VdpRegister::Unused88:
  case VdpRegister::Unused89:
  case VdpRegister::Unused8E:
    break;
  default:
    return Error{Error::InvalidWrite, fmt::format("Invalid VDP register command: {:02x}", data)};
  }
  return std::nullopt;
}

void VdpDevice::process_mode1_set(Byte value) {
  const auto mode1 = std::bit_cast<Mode1>(value);
  spdlog::debug("mode1 set freeze_hv_counter: {} enable_hblank_interrupt: {} blank_leftmost_column: {}",
                mode1.freeze_hv_counter, mode1.enable_hblank_interrupt, mode1.blank_leftmost_column);
}

void VdpDevice::process_mode2_set(Byte value) {
  const auto mode2 = std::bit_cast<Mode2>(value);
  vblank_interrupt_enabled_ = mode2.enable_vblank_interrupt;
  spdlog::debug("mode2 set vertical_resolution: {} allow_dma: {} enable_vblank_interrupt: {} enable_rendering: {}",
                magic_enum::enum_name(mode2.vertical_resolution), mode2.allow_dma, mode2.enable_vblank_interrupt,
                mode2.enable_rendering);
}

void VdpDevice::process_plane_a_table_address(Byte value) {
  const auto plane_a = std::bit_cast<PlaneATableAddress>(value);
  const AddressType address = kPlaneAddressScale * plane_a.address;
  spdlog::debug("plane A table address: {:04x}", address);
}

void VdpDevice::process_window_table_address(Byte value) {
  const auto window = std::bit_cast<WindowTableAddress>(value);
  const AddressType address = kWindowAddressScale * window.address;
  spdlog::debug("window table address: {:04x}", address);
}

void VdpDevice::process_plane_b_table_address(Byte value) {
  const auto plane_b = std::bit_cast<PlaneBTableAddress>(value);
  const AddressType address = kPlaneAddressScale * plane_b.address;
  spdlog::debug("plane B table address: {:04x}", address);
}

void VdpDevice::process_sprite_table_address(Byte value) {
  const auto sprite = std::bit_cast<SpriteTableAddress>(value);
  const AddressType address = kSpriteAddressScale * sprite.address;
  spdlog::debug("sprite table address: {:04x}", address);
}

void VdpDevice::process_background_color(Byte value) {
  const auto background = std::bit_cast<BackgroundColor>(value);
  spdlog::debug("background color palette: {} index: {}", background.palette, background.index);
}

void VdpDevice::process_hblank_interrupt_rate(Byte value) {
  spdlog::debug("hblank interrupt rate: {}", value);
}

void VdpDevice::process_mode3_set(Byte value) {
  const auto mode3 = std::bit_cast<Mode3>(value);
  spdlog::debug("mode3 set horizontal_scroll_mode: {} vertical_scroll_mode: {} enable_external_interrupt: {}",
                magic_enum::enum_name(mode3.horizontal_scroll_mode), magic_enum::enum_name(mode3.vertical_scroll_mode),
                mode3.enable_external_interrupt);
}

void VdpDevice::process_mode4_set(Byte value) {
  const auto mode4 = std::bit_cast<Mode4>(value);
  spdlog::debug("mode4 set horizontal_resolution: {} interlaced_mode: {} enable_shadow_highlight: {}",
                magic_enum::enum_name(mode4.horizontal_resolution), magic_enum::enum_name(mode4.interlaced_mode),
                mode4.enable_shadow_highlight);
}

void VdpDevice::process_hscroll_table_address(Byte value) {
  const auto hscroll = std::bit_cast<HscrollTableAddress>(value);
  const AddressType address = kHscrollAddressScale * hscroll.address;
  spdlog::debug("hscroll table address: {:04x}", address);
}

void VdpDevice::process_auto_increment(Byte value) {
  auto_increment_ = value;
  spdlog::debug("auto increment amount: {}", value);
}

void VdpDevice::process_tilemap_size(Byte value) {
  const auto tilemap = std::bit_cast<TilemapSize>(value);
  const auto to_value = [](TilemapSize::Size size) {
    switch (size) {
    case TilemapSize::Size::S32:
      return 32;
    case TilemapSize::Size::S64:
      return 64;
    case TilemapSize::Size::S128:
      return 128;
    }
  };
  tilemap_width_ = to_value(tilemap.horizontal_size);
  tilemap_height_ = to_value(tilemap.vertical_size);
  spdlog::debug("tilemap size horizontal: {} vertical: {}", magic_enum::enum_name(tilemap.horizontal_size),
                magic_enum::enum_name(tilemap.vertical_size));
}

void VdpDevice::process_window_x_division(Byte value) {
  const auto window = std::bit_cast<WindowXDivision>(value);
  spdlog::debug("window X division x_split_coordinate: {} display_to_the_right: {}", window.split_coordinate * 16,
                window.display_to_the_right);
}

void VdpDevice::process_window_y_division(Byte value) {
  const auto window = std::bit_cast<WindowYDivision>(value);
  spdlog::debug("window Y division y_split_coordinate: {} display_below: {}", window.split_coordinate * 8,
                window.display_below);
}

void VdpDevice::process_dma_length_low(Byte value) {
  dma_length_words_ &= 0xFF00;
  dma_length_words_ |= value;
  spdlog::debug("DMA length low: {:02x} current DMA length: {:04x}", value, dma_length_words_);
}

void VdpDevice::process_dma_length_high(Byte value) {
  dma_length_words_ &= 0x00FF;
  dma_length_words_ |= Word{value} << 8;
  spdlog::debug("DMA length high: {:02x} current DMA length: {:04x}", value, dma_length_words_);
}

void VdpDevice::process_dma_source_low(Byte value) {
  dma_source_words_ &= 0xFFFF00;
  dma_source_words_ |= value;
  spdlog::debug("DMA source low: {:02x} current DMA source: {:06x}", value, dma_source_words_);
}

void VdpDevice::process_dma_source_middle(Byte value) {
  dma_source_words_ &= 0xFF00FF;
  dma_source_words_ |= Long{value} << 8;
  spdlog::debug("DMA source middle: {:02x} current DMA source: {:06x}", value, dma_source_words_);
}

void VdpDevice::process_dma_source_high(Byte value) {
  using enum DmaSourceHigh::OperationType;
  const auto dma = std::bit_cast<DmaSourceHigh>(value);

  dma_source_words_ &= 0x00FFFF;
  dma_source_words_ |= Long{dma.value} << 16;
  if (dma.operation_type == DmaTransfer1) {
    dma_source_words_ |= 1 << 22;
  }

  switch (dma.operation_type) {
  case DmaTransfer0:
  case DmaTransfer1:
    dma_type_ = DmaType::MemoryToVram;
    break;
  case VramFill:
    dma_type_ = DmaType::VramFill;
    break;
  case VramCopy:
    dma_type_ = DmaType::VramCopy;
    break;
  }

  spdlog::debug("DMA source high value: {:02x} current DMA source: {:06x} operation_type: {}", dma.value,
                dma_source_words_, magic_enum::enum_name(dma.operation_type));
}

Word VdpDevice::read_status_register() {
  static const auto kStatusRegister = StatusRegister{
      .mode = StatusRegister::Mode::NTSC,
      .dma_status = StatusRegister::DmaStatus::NotBusy,
      .hblank_status = StatusRegister::HblankStatus::NotInHblank,
      .vblank_status = StatusRegister::VblankStatus::NotInVblank, // TODO: provide correct value
      .frame_status = StatusRegister::FrameStatus::EvenFrame,
      .collision_status = StatusRegister::CollisionStatus::NoCollision,
      .sprites_overflow_status = StatusRegister::SpritesOverflowStatus::NoSpritesOverflow,
      .interrupt_status = StatusRegister::InterruptStatus::InterruptNotHappened,
      .fifo_full_status = StatusRegister::FifoFullStatus::FifoNotFull,
      .fifo_empty_status = StatusRegister::FifoEmptyStatus::FifoNotEmpty,
  };
  return std::bit_cast<Word>(kStatusRegister);
}

std::vector<Byte>& VdpDevice::ram_data() {
  switch (ram_kind_) {
  case RamKind::Vram:
    return vram_data_;
  case RamKind::Vsram:
    return vsram_data_;
  case RamKind::Cram:
    return cram_data_;
  }
}

} // namespace sega

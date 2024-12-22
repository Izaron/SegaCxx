#include "lib/common/memory/device.h"
#include "lib/sega/image_saver/image_saver.h"
#include "lib/sega/memory/vdp_device.h"
#include "lib/sega/state_dump/state_dump.h"
#include "lib/sega/video/constants.h"
#include "lib/sega/video/video.h"
#include <cassert>
#include <string_view>

namespace sega {

int main(int argc, char** argv) {
  assert(argc == 3);
  const auto dump_path = std::string_view{argv[1]};
  const auto image_path = std::string_view{argv[2]};

  // make VDP device
  DummyDevice device;
  VdpDevice vdp_device{device};
  StateDump state_dump{vdp_device};
  state_dump.apply_dump_from_file(dump_path);

  // make game drawer and draw to a PNG file
  Video video{vdp_device};
  const auto data = video.raw_draw();
  save_to_png(image_path, vdp_device.tile_width() * kTileDimension, vdp_device.tile_height() * kTileDimension, data);

  return 0;
}

} // namespace sega

int main(int argc, char** argv) {
  return sega::main(argc, argv);
}

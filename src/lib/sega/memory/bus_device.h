#pragma once
#include "lib/common/error/error.h"
#include "lib/common/memory/device.h"
#include "lib/common/memory/types.h"
#include <optional>
#include <vector>

namespace sega {

class BusDevice : public Device {
public:
  struct Range {
    AddressType begin;
    AddressType end;
  };

public:
  void add_device(Range range, Device* device);

  template<typename T>
    requires(requires {
      std::derived_from<T, Device>;
      T::kBegin;
      T::kEnd;
    })
  void add_device(T* device) {
    add_device({T::kBegin, T::kEnd}, device);
  }

private:
  struct MappedDevice {
    const Range range;
    Device* device;
  };

private:
  std::optional<Error> read(AddressType addr, MutableDataView data) override;
  std::optional<Error> write(AddressType addr, DataView data) override;

  MappedDevice* find_by_addr(AddressType addr);

private:
  std::vector<MappedDevice> mapped_devices_;
};

} // namespace sega

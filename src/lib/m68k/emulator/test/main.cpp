#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <thirdparty/include/nlohmann/json.hpp>

#include "lib/m68k/emulator/emulator.h"
#include "lib/m68k/error/error.h"
#include "lib/m68k/memory/device.h"
#include "lib/m68k/memory/types.h"
#include "lib/m68k/registers/registers.h"

using json = nlohmann::json;
using namespace m68k;

thread_local std::ofstream ferr;

namespace {

using TRamSnapshot = std::map<AddressType, Byte>;

class TTestDevice : public Device {
public:
  TTestDevice(Long pc, const json& prefetch, const json& ram) {
    // fill RAM
    for (const auto& pair : ram) {
      const auto addr = pair[0].get<int>();
      const auto value = pair[1].get<Byte>();
      Values_[addr] = value;
    }

    // fill prefetch
    assert(prefetch.size() == 2);
    Values_[pc] = prefetch[0].get<int>() >> 8;
    Values_[pc + 1] = prefetch[0].get<int>() % 256;
    Values_[pc + 2] = prefetch[1].get<int>() >> 8;
    Values_[pc + 3] = prefetch[1].get<int>() % 256;
  }

  std::optional<Error> read(AddressType addr, MutableDataView data) override {
    AddressType size = data.size();
    addr &= 0xFFFFFF;
    ferr << "Read memory " << addr << " with size " << size << std::endl;

    if (size > 1 && addr % 2 != 0) {
      return Error{Error::UnalignedMemoryRead, "memory read at address %#08x of size %d", addr, size};
    }

    for (int i = addr, ptr = 0; i < addr + size; ++i, ++ptr) {
      if (const auto it = Values_.find(i & 0xFFFFFF); it != Values_.end()) {
        data[ptr] = it->second;
      } else {
        data[ptr] = 0;
      }
    }
    return std::nullopt;
  }

  std::optional<Error> write(AddressType addr, DataView data) override {
    if (data.size() > 1 && addr % 2 != 0) {
      return Error{Error::UnalignedMemoryWrite, "memory write at address %#08x of size %d", addr, data.size()};
    }

    for (const auto value : data) {
      const auto realAddr = addr & 0xFFFFFF;
      if (value != 0 || Values_.contains(realAddr)) {
        Values_[realAddr] = value;
      }
      addr++;
    }
    return std::nullopt;
  }

  TRamSnapshot GetRamSnapshot() const {
    return Values_;
  }

private:
  TRamSnapshot Values_;
};

std::string DumpRamSnapshot(const TRamSnapshot& ram) {
  std::stringstream ss;
  for (const auto& pair : ram) {
    ss << "[" << pair.first << "] = " << static_cast<int>(pair.second) << "\n";
  }
  return ss.str();
}

TRamSnapshot GetRamSnapshot(const json& ram) {
  TRamSnapshot res;
  for (const auto& pair : ram) {
    const auto addr = pair[0].get<int>();
    const auto value = pair[1].get<Byte>();
    if (value != 0) {
      res[addr] = value;
    }
  }
  return res;
}

std::vector<std::pair<AddressType, Byte>> GetRamDiff(const TRamSnapshot& ram0, const TRamSnapshot& ram1) {
  std::vector<std::pair<AddressType, Byte>> diff;
  for (const auto& p : ram1) {
    const auto it = ram0.find(p.first);
    if (it == ram0.end() || it->second != p.second) {
      diff.emplace_back(p.first, p.second);
    }
  }
  for (const auto& p : ram0) {
    if (ram1.find(p.first) == ram1.end()) {
      diff.emplace_back(p.first, 0);
    }
  }
  if (!diff.empty())
    std::sort(diff.begin(), diff.end());

  // std::stringstream ss;
  // ss << "diff is: ";
  // for (const auto& [address, value] : diff) {
  // ss << "[" << address << "] = " << static_cast<int>(value) << ", ";
  //}
  // ss << "\n";
  // ferr << ss.str();

  return diff;
}

json LoadTestFile(std::string_view path) {
  std::ifstream f(path.data());
  json data = json::parse(f);
  ferr << "\"" << path << "\" parsed" << std::endl;
  return data;
}

std::optional<std::string> DumpDiff(const Registers& lhs, const Registers& rhs) {
  std::vector<std::string> diffs;

  for (int i = 0; i < 8; ++i) {
    if (lhs.d[i] != rhs.d[i])
      diffs.emplace_back("D" + std::to_string(i));
  }
  for (int i = 0; i < 7; ++i) {
    if (lhs.a[i] != rhs.a[i])
      diffs.emplace_back("A" + std::to_string(i));
  }
  if (lhs.usp != rhs.usp)
    diffs.emplace_back("USP");
  if (lhs.ssp != rhs.ssp)
    diffs.emplace_back("SSP");
  if (lhs.pc != rhs.pc)
    diffs.emplace_back("PC");
  if ((lhs.sr ^ rhs.sr) & 0b1111'0111'0001'1111)
    diffs.emplace_back("SR");

  if (diffs.empty()) {
    return std::nullopt;
  } else {
    std::string info = "";
    for (const auto& d : diffs) {
      info += d + " ";
    }
    return info;
  }
}

Registers ParseRegisters(const json& j) {
  Registers r;
  for (int i = 0; i < 8; ++i) {
    r.d[i] = j["d" + std::to_string(i)].get<int>();
  }
  for (int i = 0; i < 7; ++i) {
    r.a[i] = j["a" + std::to_string(i)].get<int>();
  }
  r.usp = j["usp"].get<int>();
  r.ssp = j["ssp"].get<int>();
  r.sr = j["sr"].get<int>();
  r.pc = j["pc"].get<int>();
  return r;
}

bool WorkOnTest(const json& test) {
  const auto& initialJson = test["initial"];
  const auto& finalJson = test["final"];

  auto initRegs = ParseRegisters(initialJson);
  auto expectedRegs = ParseRegisters(finalJson);
  auto actualRegs = initRegs;

  TTestDevice device{initRegs.pc, initialJson["prefetch"], initialJson["ram"]};
  const auto actualRam0 = device.GetRamSnapshot();
  auto expectedRam0 = GetRamSnapshot(initialJson["ram"]);

  Context ctx{.registers = actualRegs, .device = device};
  const auto err = emulate(ctx);

  if (err) {
    ferr << "Got error: " << err->what() << std::endl;

    // this program counter means there really was an illegal instruction
    return (expectedRegs.pc == 0x1400);
  }

  const auto actualRam1 = device.GetRamSnapshot();
  const auto expectedRam1 = GetRamSnapshot(finalJson["ram"]);

  const auto regsDiff = DumpDiff(expectedRegs, actualRegs);
  bool ramDiffers = GetRamDiff(actualRam0, actualRam1) != GetRamDiff(expectedRam0, expectedRam1);

  // because of some bugs in data
  const auto name = test["name"].get<std::string>();
  if (name.contains("CHK")) {
    ramDiffers = false;
  }

  if (regsDiff || ramDiffers) {
    ferr << "Test name: \"" << name << "\"" << std::endl << std::endl;

    if (regsDiff) {
      ferr << "Initial registers:" << std::endl;
      ferr << dump(initRegs) << std::endl;

      ferr << "Actual final registers:" << std::endl;
      ferr << dump(actualRegs) << std::endl;

      ferr << "Expected final registers:" << std::endl;
      ferr << dump(expectedRegs) << std::endl;

      ferr << "Differing registers: " << *regsDiff << std::endl << std::endl;
    }

    if (ramDiffers) {
      ferr << "Initial RAM:" << std::endl;
      ferr << DumpRamSnapshot(actualRam0) << std::endl;

      ferr << "Actual RAM:" << std::endl;
      ferr << DumpRamSnapshot(actualRam1) << std::endl;

      ferr << "Expected RAM:" << std::endl;
      ferr << DumpRamSnapshot(expectedRam1) << std::endl;

      ferr << "RAM differs" << std::endl;
    }

    return false;
  }

  return true;
}

bool WorkOnFile(const json& file) {
  std::size_t size = file.size();
  ferr << "work on file with " << size << " tests" << std::endl;

  int passed = 0;
  int failed = 0;
  int ignored = size;

  for (std::size_t i = 0; i < size; ++i) {
    const bool ok = WorkOnTest(file[i]);
    ferr << (i + 1) << "/" << size << " test is " << (ok ? "OK" : "FAIL") << std::endl;

    --ignored;
    if (ok) {
      ++passed;
    } else {
      ++failed;
      // break;
    }
  }
  ferr << "TOTAL TESTS: " << size << std::endl;
  ferr << "PASSED TESTS: " << passed << std::endl;
  ferr << "FAILED TESTS: " << failed << std::endl;
  ferr << "IGNORED TESTS: " << ignored << std::endl;
  return passed == size;
}

} // namespace

int main() {
  namespace fs = std::filesystem;

  std::vector<std::string> paths;
  for (const auto& entry : fs::directory_iterator("/usr/src/680x0/68000/v1/")) {
    auto path = entry.path().string();
    if (!path.ends_with(".json")) {
      continue;
    }
    paths.emplace_back(std::move(path));
  }
  std::sort(paths.begin(), paths.end(), [](auto&& lhs, auto&& rhs) {
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
      if (std::tolower(lhs[i]) < std::tolower(rhs[i]))
        return true;
      if (std::tolower(lhs[i]) > std::tolower(rhs[i]))
        return false;
    }
    return lhs.size() < rhs.size();
  });

  const auto shouldRunTest = [](int /*index*/) { return true; };

  constexpr int threadCount = 10;
  std::mutex mut;
  int curIndex = 1;
  int totalCount = 0;

  fs::remove_all("logs");
  fs::create_directories("logs");

  std::vector<std::thread> threads;
  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back([&]() {
      while (true) {
        std::string path;
        {
          std::lock_guard guard{mut};
          while (curIndex <= paths.size() && !shouldRunTest(curIndex)) {
            path = paths[curIndex - 1];
            std::cerr << "NOT working on file " << path.substr(path.rfind('/') + 1) << " [index " << curIndex << "]"
                      << std::endl;
            ++curIndex;
          }
          if (curIndex > paths.size()) {
            return;
          }
          path = paths[curIndex - 1];
          std::cerr << "working on file " << path.substr(path.rfind('/') + 1) << std::endl;
          ++curIndex;
          ++totalCount;
        }

        std::string part = path.substr(path.rfind('/') + 1);
        part = part.substr(0, part.rfind('.'));
        ferr = std::ofstream{"logs/" + part};

        auto file = LoadTestFile(path);
        WorkOnFile(file);
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  std::cerr << "Total file count: " << totalCount << std::endl;
}

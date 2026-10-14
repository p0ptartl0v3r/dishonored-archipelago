#include "memory.hpp"

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace ap {
namespace {

std::vector<std::optional<uint8_t>> parse(std::string_view pattern) {
  std::vector<std::optional<uint8_t>> bytes;
  size_t i = 0;
  while (i < pattern.size()) {
    if (pattern[i] == ' ') {
      ++i;
      continue;
    }
    if (pattern[i] == '?') {
      bytes.emplace_back(std::nullopt);
      while (i < pattern.size() && pattern[i] == '?') ++i;
      continue;
    }
    bytes.emplace_back(static_cast<uint8_t>(std::stoul(std::string(pattern.substr(i, 2)), nullptr, 16)));
    i += 2;
  }
  return bytes;
}

}  // namespace

uintptr_t find_pattern(std::string_view pattern) {
  const auto bytes = parse(pattern);
  auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  const uint8_t* start = base + nt->OptionalHeader.BaseOfCode;
  const size_t size = nt->OptionalHeader.SizeOfCode;

  uintptr_t found = 0;
  for (size_t i = 0; i + bytes.size() <= size; ++i) {
    bool match = true;
    for (size_t j = 0; j < bytes.size(); ++j) {
      if (bytes[j] && start[i + j] != *bytes[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      if (found != 0) return 0;  // ambiguous
      found = reinterpret_cast<uintptr_t>(start + i);
    }
  }
  return found;
}

}  // namespace ap

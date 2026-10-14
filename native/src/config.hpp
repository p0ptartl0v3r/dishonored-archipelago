#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ap {

enum class CallLogMode { Off, Unique, Filter };

struct Config {
  uint32_t gnames_offset = 0x1035674;
  uint32_t gobjects_offset = 0x1023630;
  uint32_t process_event_offset = 0;  // 0 = find by pattern
  std::vector<std::string> drain_on = {"PlayerTick"};
  CallLogMode call_log = CallLogMode::Off;
  std::vector<std::string> log_filter;
};

Config load_config(const std::wstring& ini_path);

}  // namespace ap

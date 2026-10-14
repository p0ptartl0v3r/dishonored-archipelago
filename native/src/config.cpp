#include "config.hpp"

#include <windows.h>

#include <sstream>

namespace ap {
namespace {

std::string read(const std::wstring& ini, const wchar_t* section, const wchar_t* key) {
  wchar_t buffer[1024] = {};
  GetPrivateProfileStringW(section, key, L"", buffer, 1024, ini.c_str());
  std::string out;
  for (const wchar_t* c = buffer; *c; ++c) out.push_back(static_cast<char>(*c));  // values are ASCII
  return out;
}

uint32_t read_uint(const std::wstring& ini, const wchar_t* section, const wchar_t* key, uint32_t fallback) {
  const std::string text = read(ini, section, key);
  if (text.empty()) return fallback;
  try {
    return static_cast<uint32_t>(std::stoul(text, nullptr, 0));  // accepts 0x... and decimal
  } catch (...) {
    return fallback;
  }
}

std::vector<std::string> read_list(const std::wstring& ini, const wchar_t* section, const wchar_t* key,
                                   std::vector<std::string> fallback) {
  const std::string text = read(ini, section, key);
  if (text.empty()) return fallback;
  std::vector<std::string> out;
  std::stringstream stream(text);
  for (std::string item; std::getline(stream, item, ',');) {
    const auto first = item.find_first_not_of(" \t");
    const auto last = item.find_last_not_of(" \t");
    if (first != std::string::npos) out.push_back(item.substr(first, last - first + 1));
  }
  return out;
}

}  // namespace

Config load_config(const std::wstring& ini) {
  Config config;
  config.gnames_offset = read_uint(ini, L"Engine", L"GNamesOffset", config.gnames_offset);
  config.gobjects_offset = read_uint(ini, L"Engine", L"GObjectsOffset", config.gobjects_offset);
  config.process_event_offset = read_uint(ini, L"Engine", L"ProcessEventOffset", 0);
  const std::string mode = read(ini, L"Research", L"LogCalls");
  if (mode == "unique") config.call_log = CallLogMode::Unique;
  if (mode == "filter") config.call_log = CallLogMode::Filter;
  config.log_filter = read_list(ini, L"Research", L"LogFilter", {});
  return config;
}

}  // namespace ap

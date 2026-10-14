#include "log.hpp"

#include <windows.h>

#include <cstdio>
#include <mutex>

namespace ap {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}  // namespace

std::wstring log_init(const std::wstring& path) {
  std::lock_guard lock(g_mutex);
  if (g_file != nullptr) return path;
  g_file = _wfopen(path.c_str(), L"w");
  if (g_file != nullptr) return path;

  // Game folders under Program Files are often read-only.
  wchar_t local_app_data[MAX_PATH] = {};
  if (GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH) == 0) return {};
  const std::wstring folder = std::wstring(local_app_data) + L"\\DishonoredArchipelago";
  CreateDirectoryW(folder.c_str(), nullptr);
  const std::wstring fallback = folder + L"\\archipelago_native.log";
  g_file = _wfopen(fallback.c_str(), L"w");
  return g_file != nullptr ? fallback : std::wstring{};
}

void log_line(const std::string& line) {
  SYSTEMTIME t;
  GetLocalTime(&t);
  std::lock_guard lock(g_mutex);
  if (g_file == nullptr) return;
  std::fprintf(g_file, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
               line.c_str());
  std::fflush(g_file);
}

}  // namespace ap

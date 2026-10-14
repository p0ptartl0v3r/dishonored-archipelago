#include "log.hpp"

#include <windows.h>

#include <cstdio>
#include <mutex>

namespace ap {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
}  // namespace

void log_init(const std::wstring& path) {
  std::lock_guard lock(g_mutex);
  if (g_file == nullptr) g_file = _wfopen(path.c_str(), L"w");
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

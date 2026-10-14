#pragma once

#include <format>
#include <string>

namespace ap {

// Opens archipelago_native.log next to the DLL. Safe to call from any thread.
void log_init(const std::wstring& path);
void log_line(const std::string& line);

template <typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
  log_line(std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace ap

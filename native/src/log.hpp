#pragma once

#include <format>
#include <string>

namespace ap {

// Opens the log at `path`, or under %LOCALAPPDATA%\DishonoredArchipelago if that
// folder isn't writable. Returns the path actually used (empty if none worked).
std::wstring log_init(const std::wstring& path);
void log_line(const std::string& line);

template <typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
  log_line(std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace ap

#include "pipe_server.hpp"

#include <windows.h>

#include <format>
#include <string>

#include "game_hooks.hpp"
#include "log.hpp"

namespace ap {
namespace {

constexpr const wchar_t* kPipeName = L"\\\\.\\pipe\\dishonored_archipelago";

std::string handle(const std::string& request) {
  if (request == "ping") {
    switch (command_state()) {
      case CommandState::Ready: return "pong ready";
      case CommandState::NoCheats: return "pong nocheats";
      case CommandState::Waiting: break;
    }
    return "pong waiting";
  }
  if (request.rfind("console ", 0) == 0 || request.rfind("upgrade ", 0) == 0 || request == "dump") {
    queue_game_request(request);
    return "queued";
  }
  if (request.rfind("event ", 0) == 0) {
    size_t index = 0;
    try {
      index = std::stoul(request.substr(6));
    } catch (...) {
      return "error bad index";
    }
    const auto event = get_event(index);
    return event ? std::format("event {} {}", index, *event) : "none";
  }
  if (request.rfind("mark", 0) == 0) {
    log("==== MARK {} ====", request.size() > 5 ? request.substr(5) : "");
    return "ok";
  }
  return "error unknown request";
}

void serve_client(HANDLE pipe) {
  std::string buffer;
  char chunk[512];
  DWORD read = 0;
  while (ReadFile(pipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
    buffer.append(chunk, read);
    for (size_t end; (end = buffer.find('\n')) != std::string::npos;) {
      std::string request = buffer.substr(0, end);
      buffer.erase(0, end + 1);
      if (!request.empty() && request.back() == '\r') request.pop_back();
      const std::string reply = handle(request) + "\n";
      DWORD written = 0;
      if (!WriteFile(pipe, reply.data(), static_cast<DWORD>(reply.size()), &written, nullptr)) return;
    }
  }
}

DWORD WINAPI client_thread(LPVOID param) {
  HANDLE pipe = param;
  log("client connected");
  serve_client(pipe);
  log("client disconnected");
  DisconnectNamedPipe(pipe);
  CloseHandle(pipe);
  return 0;
}

// Each client gets its own pipe instance and thread, so tools/native_console.py
// works while the Archipelago client is connected.
DWORD WINAPI pipe_thread(LPVOID) {
  for (;;) {
    HANDLE pipe = CreateNamedPipeW(kPipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                   PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      log("ERROR: could not create pipe (error {})", GetLastError());
      return 1;
    }
    if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
      HANDLE thread = CreateThread(nullptr, 0, client_thread, pipe, 0, nullptr);
      if (thread != nullptr) {
        CloseHandle(thread);
        continue;
      }
      log("ERROR: could not start a client thread (error {})", GetLastError());
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
}

}  // namespace

void start_pipe_server() { CloseHandle(CreateThread(nullptr, 0, pipe_thread, nullptr, 0, nullptr)); }

}  // namespace ap

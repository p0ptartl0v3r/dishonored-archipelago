// Entry point. The game loads this as dinput8.dll; we forward the one function
// it actually uses to the real system dinput8.dll and start our own code on a
// background thread. With Dishonored Mod Loader installed, its own dinput8.dll loads
// this one instead from Binaries\Win32\NativeMods\ (any name but dinput8.dll there).

#include <windows.h>

#include <string>

#include "config.hpp"
#include "game_hooks.hpp"
#include "log.hpp"
#include "pipe_server.hpp"
#include "ue3.hpp"

namespace {

HMODULE g_module = nullptr;

std::string utf8(const wchar_t* text) {
  const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  std::string out(size > 0 ? size - 1 : 0, '\0');
  if (size > 1) WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
  return out;
}

std::wstring path_next_to_dll(const wchar_t* file_name) {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(g_module, path, MAX_PATH);
  std::wstring out(path);
  return out.substr(0, out.find_last_of(L"\\/") + 1) + file_name;
}

DWORD WINAPI hotkey_thread(LPVOID) {
  for (;;) {
    if (GetAsyncKeyState(VK_F8) & 1) ap::request_mark();
    if (GetAsyncKeyState(VK_F9) & 1) ap::toggle_call_logging();
    Sleep(50);
  }
}

DWORD WINAPI init_thread(LPVOID) {
  ap::log_init(path_next_to_dll(L"archipelago_native.log"));
  wchar_t own_path[MAX_PATH] = {};
  GetModuleFileNameW(g_module, own_path, MAX_PATH);
  ap::log("Archipelago native module loaded from {}", utf8(own_path));
  // Only one copy may run (e.g. dinput8.dll and a NativeMods copy both installed): two
  // would hook every call twice and fight over the pipe.
  const std::wstring mutex_name = L"Local\\DishonoredArchipelagoNative_" + std::to_wstring(GetCurrentProcessId());
  CreateMutexW(nullptr, FALSE, mutex_name.c_str());  // kept for the process's lifetime
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    ap::log("another copy of the Archipelago module is already running in this game; this one stays idle");
    return 0;
  }
  const ap::Config config = ap::load_config(path_next_to_dll(L"archipelago_native.ini"));

  if (!ap::install_hooks(config)) return 1;
  ap::start_pipe_server();
  CloseHandle(CreateThread(nullptr, 0, hotkey_thread, nullptr, 0, nullptr));

  // The engine fills its name/object tables during startup; wait for them.
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  std::string error;
  for (int attempt = 0; attempt < 120; ++attempt) {
    if (ue3::init(base + config.gnames_offset, base + config.gobjects_offset, error)) {
      ap::log("engine tables found");
      ap::on_engine_ready();
      return 0;
    }
    Sleep(500);
  }
  ap::log("ERROR: engine tables not usable after 60s: {}", error);
  return 1;
}

}  // namespace

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version,
                                                                   const GUID& riid, void** out, void* outer) {
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, const GUID&, void**, void*);
  static Fn real = [] {
    wchar_t path[MAX_PATH] = {};
    GetSystemDirectoryW(path, MAX_PATH);
    HMODULE system_dll = LoadLibraryW((std::wstring(path) + L"\\dinput8.dll").c_str());
    return system_dll ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(system_dll, "DirectInput8Create")))
                      : nullptr;
  }();
  return real ? real(instance, version, riid, out, outer) : E_FAIL;
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = module;
    DisableThreadLibraryCalls(module);
    CloseHandle(CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr));
  } else if (reason == DLL_PROCESS_DETACH) {
    ap::shut_down();  // nothing else: other threads are already gone, and the loader lock is held
  }
  return TRUE;
}

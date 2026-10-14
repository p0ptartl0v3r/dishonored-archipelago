#include "game_hooks.hpp"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "log.hpp"
#include "memory.hpp"
#include "ue3.hpp"

namespace ap {
namespace {

// UObject::ProcessEvent(UFunction*, void* params, void* result) is __thiscall.
// A __fastcall function with a dummy EDX argument has the same calling convention
// and, unlike __thiscall, can be written as a free function.
using ProcessEventFn = void(__fastcall*)(ue3::UObject* self, void* edx, ue3::UFunction* function, void* params,
                                         void* result);

// Byte pattern for ProcessEvent's prologue (credit: ectrc/dismod).
constexpr const char* kProcessEventPattern =
    "55 8B EC 6A ?? 68 ?? ?? ?? ?? 64 A1 ?? ?? ?? ?? 50 83 EC ?? A1 ?? ?? ?? ?? 33 C5 89 45 ?? 53 56 57 50 8D 45 ?? 64 "
    "A3 ?? ?? ?? ?? 8B F1";

struct ConsoleCommandParams {
  ue3::FString command;
  uint32_t write_to_log;
  ue3::FString return_value;
};
static_assert(offsetof(ConsoleCommandParams, return_value) == 0x10);

Config g_config;
ProcessEventFn g_original = nullptr;

std::mutex g_queue_mutex;
std::deque<std::string> g_queue;
std::atomic<bool> g_queue_pending = false;

std::atomic<bool> g_engine_ready = false;
// Looked up on the game thread (the object table can change under another thread).
std::atomic<ue3::UFunction*> g_console_command = nullptr;
ue3::UFunction* g_enable_cheats = nullptr;
ue3::UObject* g_engine = nullptr;  // lives as long as the process
ue3::UObject* g_cheats_tried_for = nullptr;  // controller we last tried EnableCheats on
DWORD g_last_resolve_tick = 0;
int g_resolve_attempts = 0;
DWORD g_last_check_tick = 0;
std::atomic<bool> g_in_gameplay = false;
thread_local bool g_inside_hook_work = false;

// Names (FName indices) of the functions that signal check-relevant events.
struct EventNames {
  int32_t destroyed = -1;                  // Actor.Destroyed: a pickup was taken (or removed on load)
  int32_t client_prepare_map_change = -1;  // PlayerController.ClientPrepareMapChange(LevelName, ...)
  int32_t on_add_abstract_item = -1;       // DishonoredPawn.OnAddAbstractItem: scripted item reward
  int32_t on_objective_action = -1;        // DishonoredPlayerController.OnObjectiveAction
  int32_t pickup_base = -1;                // class DisPickup_Base
  int32_t abstract_item_pickup = -1;       // class DisAbstractItemPickup
  int32_t whale_bone_charm = -1;           // class DisWhaleBoneCharm
};
EventNames g_names;
DWORD g_gameplay_started_tick = 0;

std::mutex g_events_mutex;
std::vector<std::string> g_events;

std::atomic<bool> g_call_logging = true;
std::mutex g_seen_mutex;
std::unordered_set<ue3::UFunction*> g_seen_functions;

bool contains_any(const std::string& text, const std::vector<std::string>& words) {
  for (const auto& word : words) {
    if (!word.empty() && text.find(word) != std::string::npos) return true;
  }
  return false;
}

void log_call(ue3::UObject* self, ue3::UFunction* function) {
  if (g_config.call_log == CallLogMode::Unique) {
    {
      std::lock_guard lock(g_seen_mutex);
      if (!g_seen_functions.insert(function).second) return;
    }
    log("call {} on {}", ue3::full_name(function), ue3::class_name(self));
    return;
  }
  // Filter mode: building names for every call is slow, which is fine for research.
  const std::string function_name = ue3::full_name(function);
  const std::string object_name = ue3::full_name(self);
  if (contains_any(function_name, g_config.log_filter) || contains_any(object_name, g_config.log_filter)) {
    log("call {} on {}", function_name, object_name);
  }
}

std::wstring widen(const std::string& text) {
  // Console commands are ASCII; MultiByteToWideChar keeps any UTF-8 intact anyway.
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring out(size > 0 ? size - 1 : 0, L'\0');
  if (size > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
  return out;
}

std::string narrow(const ue3::FString& text) {
  std::string out;
  for (int32_t i = 0; text.data != nullptr && i < text.count && text.data[i]; ++i) {
    out.push_back(static_cast<char>(text.data[i]));
  }
  return out;
}

void run_console_command(ue3::UObject* player_controller, const std::string& command) {
  const std::wstring wide = widen(command);
  ConsoleCommandParams params{};
  params.command = {wide.c_str(), static_cast<int32_t>(wide.size() + 1), static_cast<int32_t>(wide.size() + 1)};
  params.write_to_log = 1;
  g_original(player_controller, nullptr, g_console_command.load(), &params, nullptr);
  // The engine allocated return_value; we leave it rather than free it with the wrong allocator.
  const std::string result = narrow(params.return_value);
  log("console: {}{}", command, result.empty() ? "" : " -> " + result);
}

// Cheat commands (GiveRunes, AddPower, ...) live on the CheatManager, which the
// game only creates once cheats are enabled. Each map load brings a new controller.
void ensure_cheat_manager(ue3::UObject* player_controller) {
  if (ue3::field<ue3::UObject*>(player_controller, ue3::kPlayerControllerCheatManager) != nullptr) return;
  if (g_enable_cheats == nullptr || g_cheats_tried_for == player_controller) return;
  g_cheats_tried_for = player_controller;
  uint32_t no_params = 0;
  g_original(player_controller, nullptr, g_enable_cheats, &no_params, nullptr);
  const bool created = ue3::field<ue3::UObject*>(player_controller, ue3::kPlayerControllerCheatManager) != nullptr;
  log("{}", created ? "enabled cheats (created CheatManager)"
                    : "WARNING: EnableCheats did not create a CheatManager; cheat commands may be "
                      "'not recognized' until the console is enabled in-game");
}

void drain_queue(ue3::UObject* player_controller) {
  ensure_cheat_manager(player_controller);
  std::deque<std::string> commands;
  {
    std::lock_guard lock(g_queue_mutex);
    commands.swap(g_queue);
    g_queue_pending = false;
  }
  for (const auto& command : commands) run_console_command(player_controller, command);
}

// The local player's controller, read fresh from the engine each time (it is
// replaced on every map load, so it must never be cached).
ue3::UObject* local_player_controller() {
  if (g_engine == nullptr) return nullptr;
  const auto players = ue3::field<ue3::TArray<ue3::UObject*>>(g_engine, ue3::kEngineGamePlayers);
  if (players.count <= 0 || players.data == nullptr || players.data[0] == nullptr) return nullptr;
  return ue3::field<ue3::UObject*>(players.data[0], ue3::kPlayerActor);
}

void emit_event(std::string text) {
  log("event: {}", text);
  std::lock_guard lock(g_events_mutex);
  g_events.push_back(std::move(text));
}

std::string pickup_item_name(ue3::UObject* pickup) {
  auto* tweaks = ue3::field<ue3::UObject*>(pickup, ue3::kPickupTweaks);
  if (tweaks == nullptr) return "?";
  if (ue3::is_a(pickup, g_names.abstract_item_pickup)) {
    return ue3::object_name(ue3::field<ue3::UObject*>(tweaks, ue3::kAbstractPickupTweaksItem));
  }
  if (ue3::is_a(pickup, g_names.whale_bone_charm)) return ue3::object_name(tweaks);
  return "-";
}

// Called on the game thread for every ProcessEvent, before the function runs
// (so the object is still intact). Cheap integer compares unless something matches.
void detect_events(ue3::UObject* self, ue3::UFunction* function, void* params) {
  const int32_t name = function->name.index;
  if (name == g_names.destroyed) {
    if (!ue3::is_a(self, g_names.pickup_base)) return;
    // Pickups already collected in a save are destroyed while the level loads.
    const bool during_load = !g_in_gameplay || GetTickCount() - g_gameplay_started_tick < 3000;
    emit_event(std::format("pickup {} {} {}{}", ue3::path_name(self), ue3::class_name(self), pickup_item_name(self),
                           during_load ? " (load)" : ""));
  } else if (name == g_names.client_prepare_map_change) {
    emit_event("mapchange " + ue3::name_string(*static_cast<ue3::FName*>(params)));
  } else if (name == g_names.on_add_abstract_item) {
    auto* action = *static_cast<ue3::UObject**>(params);
    if (action == nullptr) return;
    auto* item = ue3::field<ue3::UObject*>(action, ue3::kAddAbstractItemActionItem);
    emit_event(std::format("additem {} {}", ue3::path_name(action), ue3::object_name(item)));
  } else if (name == g_names.on_objective_action) {
    auto* action = *static_cast<ue3::UObject**>(params);
    if (action == nullptr) return;
    const auto objectives =
        ue3::field<ue3::TArray<ue3::UObject*>>(action, ue3::kObjectiveActionObjectives);
    std::string names;
    for (int32_t i = 0; i < objectives.count && objectives.data != nullptr; ++i) {
      names += (names.empty() ? "" : ",") + ue3::object_name(objectives.data[i]);
    }
    emit_event(std::format("objective {} {} {}", ue3::path_name(action), ue3::class_name(action),
                           names.empty() ? "-" : names));
  }
}

// Finds the engine object and ConsoleCommand. Retried until they are loaded.
void resolve_engine_objects() {
  const DWORD now = GetTickCount();
  if (now - g_last_resolve_tick < 1000) return;
  g_last_resolve_tick = now;

  if (g_engine == nullptr) g_engine = ue3::find_instance("DishonoredEngine");
  ue3::UFunction* console_command = ue3::find_object("Function Engine.PlayerController.ConsoleCommand");
  if (g_engine == nullptr || console_command == nullptr) {
    if (++g_resolve_attempts == 30) {
      log("WARNING: after 30s still missing:{}{}", g_engine ? "" : " DishonoredEngine",
          console_command ? "" : " PlayerController.ConsoleCommand");
    }
    return;
  }
  g_names.destroyed = ue3::find_name_index("Destroyed");
  g_names.client_prepare_map_change = ue3::find_name_index("ClientPrepareMapChange");
  g_names.on_add_abstract_item = ue3::find_name_index("OnAddAbstractItem");
  g_names.on_objective_action = ue3::find_name_index("OnObjectiveAction");
  g_names.pickup_base = ue3::find_name_index("DisPickup_Base");
  g_names.abstract_item_pickup = ue3::find_name_index("DisAbstractItemPickup");
  g_names.whale_bone_charm = ue3::find_name_index("DisWhaleBoneCharm");
  g_enable_cheats = ue3::find_object("Function Engine.PlayerController.EnableCheats");
  if (g_enable_cheats == nullptr) log("WARNING: PlayerController.EnableCheats not found");
  g_console_command = console_command;
  log("found DishonoredEngine ({}) and PlayerController.ConsoleCommand", ue3::object_name(g_engine));
}

void __fastcall process_event_hook(ue3::UObject* self, void* edx, ue3::UFunction* function, void* params,
                                   void* result) {
  if (self != nullptr && function != nullptr && g_engine_ready && !g_inside_hook_work) {
    g_inside_hook_work = true;
    if (g_console_command == nullptr) resolve_engine_objects();
    if (g_config.call_log != CallLogMode::Off && g_call_logging) log_call(self, function);
    if (g_console_command != nullptr) detect_events(self, function, params);
    // ProcessEvent runs thousands of times a frame; look at the player at most every 100ms.
    const DWORD now = GetTickCount();
    if (g_console_command != nullptr && now - g_last_check_tick >= 100) {
      g_last_check_tick = now;
      ue3::UObject* controller = local_player_controller();
      // A pawn means we're playing, not in the main menu or a loading screen.
      const bool in_gameplay = controller != nullptr && ue3::field<ue3::UObject*>(controller, ue3::kControllerPawn);
      if (in_gameplay && !g_in_gameplay) {
        g_gameplay_started_tick = now;
        emit_event("gameplay " + ue3::package_name(controller));  // the map we're playing in
      }
      if (in_gameplay != g_in_gameplay) log("{}", in_gameplay ? "in gameplay" : "not in gameplay");
      if (!in_gameplay) g_cheats_tried_for = nullptr;  // a new controller may reuse the old address
      g_in_gameplay = in_gameplay;
      if (in_gameplay && g_queue_pending) drain_queue(controller);
    }
    g_inside_hook_work = false;
  }
  g_original(self, edx, function, params, result);
}

}  // namespace

bool install_hooks(const Config& config) {
  g_config = config;
  uintptr_t target = 0;
  if (config.process_event_offset != 0) {
    target = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + config.process_event_offset;
  } else {
    target = find_pattern(kProcessEventPattern);
  }
  if (target == 0) {
    log("ERROR: ProcessEvent not found (pattern did not match exactly once). Set ProcessEventOffset.");
    return false;
  }
  log("ProcessEvent at exe+0x{:X}", target - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));

  if (MH_Initialize() != MH_OK ||
      MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&process_event_hook),
                    reinterpret_cast<void**>(&g_original)) != MH_OK ||
      MH_EnableHook(reinterpret_cast<void*>(target)) != MH_OK) {
    log("ERROR: failed to hook ProcessEvent");
    return false;
  }
  return true;
}

void on_engine_ready() { g_engine_ready = true; }

void queue_console_command(const std::string& command) {
  std::lock_guard lock(g_queue_mutex);
  g_queue.push_back(command);
  g_queue_pending = true;
}

bool commands_ready() { return g_console_command != nullptr && g_in_gameplay; }

void toggle_call_logging() {
  g_call_logging = !g_call_logging;
  log("---- call logging {} ----", g_call_logging ? "resumed" : "paused");
}

std::optional<std::string> get_event(size_t index) {
  std::lock_guard lock(g_events_mutex);
  if (index >= g_events.size()) return std::nullopt;
  return g_events[index];
}

}  // namespace ap

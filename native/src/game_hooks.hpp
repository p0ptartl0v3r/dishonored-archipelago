#pragma once

#include <optional>
#include <string>

#include "config.hpp"

namespace ap {

// Hooks UObject::ProcessEvent. Returns false (and logs why) on failure.
bool install_hooks(const Config& config);

// Call once the engine's name/object tables are valid. Lookups then happen on the game thread.
void on_engine_ready();

// Queue a console command; it runs on the game thread within ~100ms while in gameplay.
void queue_console_command(const std::string& command);

enum class CommandState {
  Waiting,   // not in gameplay (menu, loading) or engine objects not found yet
  NoCheats,  // in gameplay, but cheats couldn't be enabled, so cheat commands would fail
  Ready,
};
CommandState command_state();

void toggle_call_logging();

// Game events seen since the game started (pickups, map changes, ...), in order.
// Index-based so a reconnecting client can replay from 0.
std::optional<std::string> get_event(size_t index);

}  // namespace ap

#pragma once

#include <optional>
#include <string>

#include "config.hpp"

namespace ap {

// Hooks UObject::ProcessEvent. Returns false (and logs why) on failure.
bool install_hooks(const Config& config);

// Call once the engine's name/object tables are valid. Lookups then happen on the game thread.
void on_engine_ready();

// Queue a request for the game thread; it runs within ~100ms once in gameplay with cheats on:
//   "console <command>"  run a console command
//   "upgrade <name>"     give the DisTweaks_Upgrade with that object name (skipped if owned)
//   "dump"               log upgrades, bone charm slots, inventory and ammo (research)
void queue_game_request(const std::string& request);

enum class CommandState {
  Waiting,   // not in gameplay (menu, loading) or engine objects not found yet
  NoCheats,  // in gameplay, but cheats couldn't be enabled, so cheat commands would fail
  Ready,
};
CommandState command_state();

void toggle_call_logging();

// What the player may have: kind "power" (name, highest level) or "upgrade" (name, 0/1).
// Once any limit is set, the DLL blocks and removes randomized powers/upgrades beyond them.
bool set_limit(const std::string& kind, const std::string& name, int32_t value);

// Show a line of text on screen for a few seconds (drawn over the HUD).
void show_message(const std::string& text);
// One line shown at the top of the main menu (why the player was sent there).
void show_notice(const std::string& text);

// Write a marker with the player's position to the log (on the game thread, soon).
void request_mark();

// Stop doing any work in the hook (the process is exiting).
void shut_down();

// Game events seen since the game started (pickups, map changes, ...), in order.
// Index-based so a reconnecting client can replay from 0.
std::optional<std::string> get_event(size_t index);

}  // namespace ap

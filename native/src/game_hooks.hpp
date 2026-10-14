#pragma once

#include <string>

#include "config.hpp"

namespace ap {

// Hooks UObject::ProcessEvent. Returns false (and logs why) on failure.
bool install_hooks(const Config& config);

// Call once the engine's name/object tables are valid. Lookups then happen on the game thread.
void on_engine_ready();

// Queue a console command; it runs on the game thread at the next drain point.
void queue_console_command(const std::string& command);

// True when commands can run: ConsoleCommand was found and a drain point fired recently.
bool commands_ready();

void toggle_call_logging();

}  // namespace ap

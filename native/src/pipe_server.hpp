#pragma once

namespace ap {

// Serves \\.\pipe\dishonored_archipelago on a background thread.
//
// Line-based protocol (UTF-8, '\n' terminated), one client at a time:
//   ping              -> "pong ready" | "pong waiting"
//   console <command> -> "queued"     (runs on the game thread soon after)
//   mark <text>       -> "ok"         (writes a marker line to the log)
// Anything else       -> "error unknown request"
void start_pipe_server();

}  // namespace ap

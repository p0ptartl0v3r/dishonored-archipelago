#pragma once

namespace ap {

// Serves \\.\pipe\dishonored_archipelago on a background thread.
//
// Line-based protocol (UTF-8, '\n' terminated), several clients at once:
//   ping              -> "pong ready" | "pong waiting" (menu/loading) | "pong nocheats" (cheats off)
//   console <command> -> "queued"     (runs on the game thread soon after)
//   upgrade <name>    -> "queued"     (gives that DisTweaks_Upgrade unless already owned)
//   dump              -> "queued"     (logs upgrades, bone charm slots, powers, inventory, ammo)
//   limit power <Name> <max level> | limit upgrade <name> <0|1> -> "ok"
//                                     (what the player may have; enables blocking and removal)
//   event <n>         -> "event <n> <text>" | "none"   (n-th game event since start, from 0)
//   mark <text>       -> "ok"         (writes a marker line to the log)
//   message <text>    -> "ok"         (shows the text on screen for a few seconds)
// Anything else       -> "error unknown request"
void start_pipe_server();

}  // namespace ap

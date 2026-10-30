# Research

Notes from reverse engineering the game. Treat this folder as a lab notebook:
write everything down, including dead ends.

- `notes.md`: game build, classes and functions found in UE Explorer, console
  commands, memory addresses and pointer paths (always record the game build).
- `locations.csv`: one row per location, generated from the data files, with
  `detection` (the event kind that completes it), `game_object` (the in-game object,
  script action or journal entry) and notes. Every row is filled in; the `detect`
  patterns in `worlds/dishonored/data/locations.json` are what the client actually uses.
  Don't edit `location_id`; it comes from `worlds/dishonored/data/ids.json`.
- `prior-art.md`: other Dishonored mods surveyed before starting (2026-10-14).
- Cheat Engine tables (`*.CT`) can live here too.

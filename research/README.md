# Research

Notes from reverse engineering the game. Treat this folder as a lab notebook:
write everything down, including dead ends.

- `notes.md`: game build, classes and functions found in UE Explorer, console
  commands, memory addresses and pointer paths (always record the game build).
- `locations.csv`: one row per location, generated from the data files. Fill in
  `detection` (how the client can tell it is done) and `game_object` (the in-game
  object/actor name for the pickup) as you find them. Don't edit `location_id`;
  it comes from `worlds/dishonored/data/ids.json`.
- Cheat Engine tables (`*.CT`) can live here too.

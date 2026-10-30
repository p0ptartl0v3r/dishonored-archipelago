# Dishonored Archipelago

An [Archipelago](https://archipelago.gg) multiworld randomizer integration for
*Dishonored* (2012, PC).

Powers, equipment, upgrades, bone charm slots and zone keys are shuffled. Runes,
bone charms, paintings, blueprints and mission completions are the checks. The
item and location design comes from the Manual version by greenranger1200 (see `manual/`).

**Status:** playable start to finish with the in-game DLL from `native/` (one full
seed played to its goal on the Steam build, low chaos). The client and DLL:

- grant received powers, upgrades, equipment and bone charm slots, and take away or
  block what hasn't been received (shop purchases, script grants, old saves);
- detect all 100 locations automatically, including mission victories and the goal;
- enforce zone keys: travel prompts into a zone without its key are refused, and
  anywhere else the player is sent back to the main menu;
- show items sent and received, and Archipelago chat, in-game.

Without the DLL, the client plays the same as the Manual (`/check`, `/goal`, items
listed for you to apply by hand). Not verified yet: high-chaos levels, other game
languages, GOG/Epic builds. See [docs/roadmap.md](docs/roadmap.md).

## Layout

```
worlds/dishonored/      the apworld (copy or link into Archipelago's worlds/)
  data/                 items, locations, regions (Manual format) + ids.json
  client/               Dishonored Client; game_interface.py is the game boundary
  test/                 tests that run inside Archipelago
tests/                  tests that run without Archipelago
tools/                  sync_ids.py, build_apworld.py, check_log.py, native_console.py
research/               reverse-engineering notes and the location table
native/                 in-game DLL (dinput8.dll proxy): grants, blocking, events, messages
manual/                 original Manual apworld and docs
docs/                   dev environment and roadmap
```

## Quick start

See [docs/dev-environment.md](docs/dev-environment.md) for the full setup.

```bash
python -m pytest tests              # quick checks, no Archipelago needed
python tools/build_apworld.py       # -> dist/dishonored.apworld
```

Install `dist/dishonored.apworld` into Archipelago (`custom_worlds/`), generate
with a YAML like:

```yaml
name: Corvo
game: Dishonored
Dishonored:
  void_walkers_arsenal: true   # false if you don't own the DLC
  starting_zone_keys: none     # none / prison / intro
  death_link: false
```

Install the DLL (`dinput8.dll` and `archipelago_native.ini` into the game's
`Binaries\Win32`; see [native/README.md](native/README.md)), then open
**Dishonored Client** from the Archipelago Launcher. Players' rules (choices that
keep every check reachable, options) are in
[worlds/dishonored/docs/en_Dishonored.md](worlds/dishonored/docs/en_Dishonored.md).

## Adding or renaming items and locations

Edit `worlds/dishonored/data/*.json`, then run `python tools/sync_ids.py`.
IDs are append-only: once a game build ships with an ID, never reuse or change it.
To make a location detected, add a `detect` pattern (event formats in
[native/README.md](native/README.md)) and check it against a play log with
`python tools/check_log.py "<mission>"`.

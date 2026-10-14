# Dishonored Archipelago

An [Archipelago](https://archipelago.gg) multiworld randomizer integration for
*Dishonored* (2012, PC).

Powers, equipment, upgrades, bone charm slots and zone keys are shuffled. Runes,
bone charms, paintings, blueprints and mission completions are the checks. The
item and location design comes from the Manual version by greenranger1200 (see `manual/`).

**Status:** the apworld generates seeds, and the client connects and plays the
same as the Manual (send checks with `/check`, items are listed for you to apply
by hand). With the in-game DLL from `native/` installed, the client applies
powers automatically and sends the locations that have `detect` patterns
(a handful so far). See
[docs/roadmap.md](docs/roadmap.md).

## Layout

```
worlds/dishonored/      the apworld (copy or link into Archipelago's worlds/)
  data/                 items, locations, regions (Manual format) + ids.json
  client/               Dishonored Client; game_interface.py is the game boundary
  test/                 tests that run inside Archipelago
tests/                  tests that run without Archipelago
tools/                  sync_ids.py, build_apworld.py
research/               reverse-engineering notes and the location table
native/                 in-game DLL (dinput8.dll proxy): runs console commands, logs calls
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
  death_link: false
```

and open **Dishonored Client** from the Archipelago Launcher.

## Adding or renaming items and locations

Edit `worlds/dishonored/data/*.json`, then run `python tools/sync_ids.py`.
IDs are append-only: once a game build ships with an ID, never reuse or change it.

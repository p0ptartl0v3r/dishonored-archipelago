# Dishonored Archipelago

An [Archipelago](https://archipelago.gg) multiworld randomizer integration for
*Dishonored* (2012, PC).

Powers, equipment, upgrades, bone charm slots and zone keys are shuffled. Runes,
bone charms, paintings, blueprints and mission completions are the checks.

**Logic built off the Manual created by greenranger1200** (their Manual apworld for
Dishonored, kept for reference in `manual/`), used with their permission.

**Status:** 0.9.0 beta, [released](https://github.com/p0ptartl0v3r/dishonored-archipelago/releases).
Playable start to finish with the in-game DLL from `native/` (one full seed played to its
goal on the Steam build; every location also verified at high chaos). The client and DLL:

- grant received powers, upgrades, equipment and bone charm slots, and take away or
  block what hasn't been received (shop purchases, script grants, old saves);
- detect all 100 locations automatically, including mission victories and the goal;
- enforce zone keys: travel prompts into a zone without its key are refused, and
  anywhere else the player is sent back to the main menu;
- show items sent and received, and Archipelago chat, in-game.

Without the DLL, the client plays the same as the Manual (`/check`, `/goal`, items
listed for you to apply by hand). Not verified yet: other game languages, GOG/Epic
builds. See [docs/roadmap.md](docs/roadmap.md).

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

## Releasing

1. Set `world_version` in `worlds/dishonored/archipelago.json` to the new version, and
   write the release text in `docs/releases/<version>.md` (optional: without it, GitHub
   generates notes from the commits).
2. Merge to `main`, then tag that commit and push the tag:
   ```bash
   git tag v0.9.0
   git push origin v0.9.0
   ```
3. CI runs the tests and builds, checks that the tag matches `world_version`, and publishes
   a GitHub release with `dishonored.apworld` and the DLL zips (normal install, and one for
   Dishonored Mod Loader). Versions below 1.0 are marked as pre-releases.

## Adding or renaming items and locations

Edit `worlds/dishonored/data/*.json`, then run `python tools/sync_ids.py`.
IDs are append-only: once a game build ships with an ID, never reuse or change it.
To make a location detected, add a `detect` pattern (event formats in
[native/README.md](native/README.md)) and check it against a play log with
`python tools/check_log.py "<mission>"`.

## License

The code and documentation in this repository are under the [MIT License](LICENSE), except:

- **The Manual's design and files are greenranger1200's work**, used and included with their
  permission. That covers the item, location and region design in `worlds/dishonored/data/`
  (taken from their Manual for Dishonored), and everything in `manual/`: their Manual
  apworld, its YAML template and the documentation PDF.
- **The Manual framework** ([Manual for Archipelago](https://github.com/ManualForArchipelago/Manual)),
  which the Manual apworld in `manual/` is built on, is under the MIT License, © 2023 Manual
  for Archipelago; see [manual/MANUAL_FRAMEWORK_LICENSE.txt](manual/MANUAL_FRAMEWORK_LICENSE.txt).
  The apworld in `worlds/dishonored/` uses the Manual's data format but none of its code.
- **The in-game module includes MinHook** (BSD 2-Clause); see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The release zips carry the same notice.
- **The engine layout facts** (offsets, class names) are credited to ectrc/dismod. No code
  from it is used.

Dishonored is a trademark of ZeniMax Media Inc. This project is unofficial, and contains no
files from the game.

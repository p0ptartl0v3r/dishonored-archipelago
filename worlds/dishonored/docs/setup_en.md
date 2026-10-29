# Dishonored Setup Guide

## Required Software

- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases)
- Dishonored (PC)

## Installation

1. Put `dishonored.apworld` in the `custom_worlds` folder of your Archipelago install
   (or double-click it to install).
2. Open the Archipelago Launcher and choose **Dishonored Client**.

## Playing

1. Install the in-game module: copy `dinput8.dll` and `archipelago_native.ini`
   next to `Dishonored.exe` (in `Binaries\Win32`). Tested on the Steam version.
2. Open **Dishonored Client** from the Archipelago Launcher and connect.
3. Start Dishonored. The client says "Connected to the Dishonored native module".

Received items are applied in-game, and items you haven't received yet are taken
away or blocked. Every location is detected automatically, and finishing *The Light
at the End* sends your goal. If a check ever isn't detected, send it yourself with
`/check <location name>` (or `/goal`); both work even when the game isn't running.

Read the game's info page for the choices that keep every check
reachable and how zone keys work. Powers are granted directly, so you don't need the
Manual's console runes to unlock them. To uninstall the module, delete `dinput8.dll`.

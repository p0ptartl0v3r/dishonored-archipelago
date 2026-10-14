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
   next to `Dishonored.exe` (in `Binaries\Win32`).
2. Open **Dishonored Client** from the Archipelago Launcher and connect.
3. Start Dishonored. The client says "Connected to the Dishonored native module".

Game integration is still in development. Received powers are applied
automatically; other items are listed for you to apply by hand. Only some
locations are detected automatically so far: send the rest yourself with
`/check <location name>`, and type `/goal` once you finish *The Light at the End*.
Both commands work even when the game isn't running.

Read the rules in the Manual documentation (choices you must make so every check
stays reachable, and how to give yourself runes from the console).

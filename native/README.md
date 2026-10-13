# Native (injected DLL)

Nothing here yet, by design. Start once the research shows that reading memory from
outside isn't enough (most likely for telling *which* pickup was taken, and for
blocking purchases).

Planned shape:

- A **32-bit** (Win32/x86) C++ DLL built with Visual Studio 2022 and CMake.
- Loaded by a proxy DLL the game already loads (for example a `dinput8.dll` or
  `version.dll` proxy), so players need no separate injector.
- Finds Unreal Engine 3's global object and name tables and hooks
  `UObject::ProcessEvent` (with MinHook). This is the standard way to mod UE3
  games: it lets you see every UnrealScript function call (pickups, level loads,
  purchases) and call functions yourself (give items, run console commands).
- Talks to the Python client over a named pipe, as a third `GameInterface`
  backend. The DLL stays small and the Archipelago logic stays in Python.

"""Talk to the in-game DLL directly, without Archipelago (Windows only).

    python tools/native_console.py ping
    python tools/native_console.py "console AddPower Blink 1"
    python tools/native_console.py "mark before rune"
    python tools/native_console.py            # interactive: type requests, Ctrl+C to quit
"""

from __future__ import annotations

import sys

PIPE_PATH = r"\\.\pipe\dishonored_archipelago"


def request(pipe, line: str) -> str:
    pipe.write((line + "\n").encode("utf-8"))
    reply = bytearray()
    while not reply.endswith(b"\n"):
        chunk = pipe.read(1)
        if not chunk:
            raise OSError("pipe closed")
        reply += chunk
    return reply.decode("utf-8").strip()


def main() -> int:
    try:
        pipe = open(PIPE_PATH, "r+b", buffering=0)
    except OSError as error:
        print(f"Could not open {PIPE_PATH}: {error}. Is the game running with dinput8.dll installed?")
        return 1
    with pipe:
        if len(sys.argv) > 1:
            print(request(pipe, " ".join(sys.argv[1:])))
            return 0
        try:
            while True:
                print(request(pipe, input("> ")))
        except (KeyboardInterrupt, EOFError):
            return 0


if __name__ == "__main__":
    sys.exit(main())

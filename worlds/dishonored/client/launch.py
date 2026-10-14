import asyncio
from collections.abc import Sequence

import colorama

from CommonClient import get_base_parser, handle_url_arg


def launch_dishonored_client(*args: Sequence[str]) -> None:
    from .dishonored_client import main

    parser = get_base_parser(description="Dishonored Archipelago Client")
    parser.add_argument("--name", default=None, help="Slot name to connect as.")
    parser.add_argument(
        "--backend",
        choices=("manual", "memory", "native"),
        default="native",
        help="How to talk to the game. 'native' (default) uses the in-game DLL from native/; "
        "'manual' never touches the game (use /check, like the Manual).",
    )
    parser.add_argument("url", nargs="?", help="Archipelago connection url")
    launch_args = handle_url_arg(parser.parse_args(args), parser=parser)

    colorama.just_fix_windows_console()
    asyncio.run(main(launch_args))
    colorama.deinit()

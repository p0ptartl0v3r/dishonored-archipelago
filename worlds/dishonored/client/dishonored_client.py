from __future__ import annotations

import asyncio
import logging
from typing import Any

from CommonClient import ClientCommandProcessor, CommonContext, gui_enabled, logger, server_loop
from NetUtils import ClientStatus

from ..game_data import GAME_NAME
from .game_interface import BACKENDS, GameInterface

POLL_SECONDS = 0.5


class DishonoredCommandProcessor(ClientCommandProcessor):
    ctx: "DishonoredContext"

    def _cmd_check(self, *name: str) -> bool:
        """Send a location by (partial) name, for things the game can't detect yet."""
        query = " ".join(name).strip().lower()
        if not query:
            self.output("Usage: /check <location name>")
            return False
        if not self.ctx.slot:
            self.output("Not connected.")
            return False
        names = self.ctx.location_names
        candidates = [
            loc_id for loc_id in self.ctx.missing_locations if query in names.lookup_in_game(loc_id).lower()
        ]
        exact = [loc_id for loc_id in candidates if names.lookup_in_game(loc_id).lower() == query]
        if exact:
            candidates = exact
        if len(candidates) != 1:
            self.output(f"{len(candidates)} unchecked locations match {query!r}:")
            for loc_id in sorted(candidates)[:20]:
                self.output(f"  {names.lookup_in_game(loc_id)}")
            return False
        self.ctx.pending_checks.add(candidates[0])
        return True

    def _cmd_goal(self) -> bool:
        """Tell the server you have finished the game."""
        self.ctx.goal_reached = True
        return True

    def _cmd_backend(self) -> bool:
        """Show which game backend is in use and whether it is attached."""
        game = self.ctx.game_interface
        self.output(f"Backend: {game.name}, attached: {game.is_attached()}")
        return True


class DishonoredContext(CommonContext):
    game = GAME_NAME
    items_handling = 0b111  # receive every item, including our own and starting inventory
    command_processor = DishonoredCommandProcessor

    def __init__(self, server_address: str | None, password: str | None, game_interface: GameInterface) -> None:
        super().__init__(server_address, password)
        self.game_interface = game_interface
        self.pending_checks: set[int] = set()
        self.goal_reached = False
        self.goal_sent = False
        # Index into items_received of the next item to apply. TODO: once items are
        # really applied in-game, this must be stored in the save file instead, or
        # loading an old save will hand out items twice / not at all.
        self.items_applied = 0
        self.slot_data: dict[str, Any] = {}

    async def server_auth(self, password_requested: bool = False) -> None:
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict) -> None:
        if cmd == "Connected":
            self.slot_data = args.get("slot_data", {})
            self.items_applied = 0
            self.goal_sent = False

    async def disconnect(self, allow_autoreconnect: bool = False) -> None:
        self.items_applied = 0
        await super().disconnect(allow_autoreconnect)

    def make_gui(self):
        ui = super().make_gui()
        ui.base_title = "Archipelago Dishonored Client"
        return ui


async def game_loop(ctx: DishonoredContext) -> None:
    game = ctx.game_interface
    waiting_logged = False
    next_attach = 0.0
    while not ctx.exit_event.is_set():
        try:
            attached = game.is_attached()
            if not attached and asyncio.get_running_loop().time() >= next_attach:
                attached = game.attach()
                if attached:
                    logger.info(f"Game backend '{game.name}' attached")
                    waiting_logged = False
                else:
                    next_attach = asyncio.get_running_loop().time() + 2
                    if not waiting_logged:
                        logger.info("Waiting for Dishonored (with the Archipelago dinput8.dll installed)...")
                        waiting_logged = True

            if ctx.slot:
                if attached:
                    await sync_with_game(ctx, game)
                # /check and /goal work even while the game isn't running.
                await send_checks_and_goal(ctx, set(), False)
        except Exception:
            logger.exception("Error in game loop")
            game.detach()
            await asyncio.sleep(2)
        await asyncio.sleep(POLL_SECONDS)


async def send_checks_and_goal(ctx: DishonoredContext, found: set[int], goal: bool) -> None:
    found = found | ctx.pending_checks
    ctx.pending_checks.clear()
    new = found - ctx.locations_checked
    if new:
        await ctx.check_locations(new)

    if (ctx.goal_reached or goal) and not ctx.goal_sent:
        await ctx.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}])
        ctx.finished_game = True
        ctx.goal_sent = True
        logger.info("Goal sent!")


async def sync_with_game(ctx: DishonoredContext, game: GameInterface) -> None:
    await send_checks_and_goal(ctx, game.poll_checked_locations(), game.poll_goal_reached())

    # Received items wait here until the game can take them (give_item returns False).
    while ctx.items_applied < len(ctx.items_received):
        net_item = ctx.items_received[ctx.items_applied]
        name = ctx.item_names.lookup_in_game(net_item.item)
        level = sum(1 for i in ctx.items_received[: ctx.items_applied + 1] if i.item == net_item.item)
        if not game.give_item(net_item.item, name, level):
            break
        ctx.items_applied += 1


async def main(args) -> None:
    logging.getLogger().setLevel(logging.INFO)
    ctx = DishonoredContext(args.connect, args.password, BACKENDS[args.backend]())
    ctx.auth = args.name
    ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
    loop_task = asyncio.create_task(game_loop(ctx), name="game loop")

    if gui_enabled:
        ctx.run_gui()
    ctx.run_cli()

    await ctx.exit_event.wait()
    loop_task.cancel()
    ctx.game_interface.detach()
    await ctx.shutdown()

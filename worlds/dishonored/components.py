from worlds.LauncherComponents import Component, Type, components, launch


def run_client(*args: str) -> None:
    from .client.launch import launch_dishonored_client

    launch(launch_dishonored_client, name="Dishonored Client", args=args)


components.append(
    Component(
        "Dishonored Client",
        func=run_client,
        game_name="Dishonored",
        component_type=Type.CLIENT,
        supports_uri=True,
    )
)

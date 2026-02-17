"""Level/map management tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_level_tools(mcp: FastMCP) -> None:
    """Register all level management MCP tools."""

    @mcp.tool()
    async def get_level_info() -> str:
        """Get information about the current world: map name, persistent level, and all streaming sub-levels.

        Returns each sub-level's package name, visibility, loaded state, actor count,
        and transform (location/rotation).

        Returns:
            JSON with world_name, map_path, persistent_level, current_level,
            streaming_levels array, and streaming_level_count
        """
        result = await send_command("get_level_info", {})
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def create_level(
        save_path: str = "",
        template_path: str = "",
        save_existing: bool = True,
    ) -> str:
        """Create a new blank map or from a template.

        Args:
            save_path: Optional path to save the new map (e.g., '/Game/Maps/NewLevel').
                If empty, the map is created but not saved to disk.
            template_path: Optional template map asset path to base the new level on.
                If empty, creates a blank map.
            save_existing: Whether to save the current map before creating the new one (default: True)

        Returns:
            JSON with the new world's name, map path, and save status
        """
        params: dict = {"save_existing": save_existing}
        if save_path:
            params["save_path"] = save_path
        if template_path:
            params["template_path"] = template_path

        result = await send_command("create_level", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def save_level(
        asset_path: str = "",
        save_all: bool = False,
    ) -> str:
        """Save the current map, save as to a new path, or save all dirty packages.

        Args:
            asset_path: If provided, saves the map to this path (Save As).
                Example: '/Game/Maps/MyLevel'
            save_all: If True, saves all dirty map and content packages (default: False)

        Returns:
            JSON with save result details
        """
        params: dict = {}
        if asset_path:
            params["asset_path"] = asset_path
        if save_all:
            params["save_all"] = True

        result = await send_command("save_level", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def load_level(
        map_path: str,
        save_existing: bool = True,
    ) -> str:
        """Open an existing map in the editor.

        Args:
            map_path: Asset path of the map to load (e.g., '/Game/Maps/MyLevel')
            save_existing: Whether to save the current map before loading (default: True)

        Returns:
            JSON with the loaded world's name, map path, actor count, and streaming level count
        """
        result = await send_command("load_level", {
            "map_path": map_path,
            "save_existing": save_existing,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_streaming_level(
        package_name: str,
        streaming_class: str = "Dynamic",
        location: list[float] | None = None,
        rotation: list[float] | None = None,
        create_new: bool = False,
    ) -> str:
        """Add a streaming sub-level to the current world.

        Can either add an existing level or create a new empty one.

        Args:
            package_name: Package name/path of the level.
                For existing: '/Game/Maps/SubLevel1'
                For new: desired name like '/Game/Maps/NewSubLevel'
            streaming_class: 'Dynamic' (load/unload at runtime) or
                'AlwaysLoaded' (always present). Default: 'Dynamic'
            location: Optional world offset [x, y, z] for the sub-level
            rotation: Optional rotation [pitch, yaw, roll] for the sub-level
            create_new: If True, creates a new empty level instead of
                referencing an existing one (default: False)

        Returns:
            JSON with the streaming level's package name, class, and creation status
        """
        params: dict = {
            "package_name": package_name,
            "streaming_class": streaming_class,
            "create_new": create_new,
        }
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation

        result = await send_command("add_streaming_level", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def remove_streaming_level(package_name: str) -> str:
        """Remove a streaming sub-level from the current world.

        The level must be loaded. This removes it from the world but does not
        delete the level asset from disk.

        Args:
            package_name: Package name of the streaming level to remove
                (e.g., '/Game/Maps/SubLevel1' or just 'SubLevel1')

        Returns:
            JSON confirming the removal
        """
        result = await send_command("remove_streaming_level", {
            "package_name": package_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_level_visibility(
        package_name: str,
        visible: bool = True,
        make_current: bool = False,
    ) -> str:
        """Show or hide a streaming sub-level, and optionally make it the current editing level.

        Args:
            package_name: Package name of the streaming level
                (e.g., '/Game/Maps/SubLevel1' or just 'SubLevel1')
            visible: Whether the level should be visible (default: True)
            make_current: If True and visible=True, makes this the current level
                for editing (new actors will be placed here). Default: False

        Returns:
            JSON with the level's visibility state and whether it's current
        """
        result = await send_command("set_level_visibility", {
            "package_name": package_name,
            "visible": visible,
            "make_current": make_current,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

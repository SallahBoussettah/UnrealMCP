"""Actor management tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_actor_tools(mcp: FastMCP) -> None:
    """Register all actor management MCP tools."""

    @mcp.tool()
    async def spawn_actor(
        actor_class: str = "StaticMeshActor",
        name: str = "",
        location: list[float] | None = None,
        rotation: list[float] | None = None,
        scale: list[float] | None = None,
        blueprint_path: str = "",
    ) -> str:
        """Spawn an actor in the current level.

        Args:
            actor_class: Class of actor to spawn (e.g., 'StaticMeshActor', 'PointLight', 'CameraActor').
                Ignored if blueprint_path is provided.
            name: Optional name/label for the actor
            location: World location [x, y, z] (default: [0, 0, 0])
            rotation: World rotation [pitch, yaw, roll] in degrees (default: [0, 0, 0])
            scale: World scale [x, y, z] (default: [1, 1, 1])
            blueprint_path: If provided, spawn an instance of this Blueprint instead of actor_class

        Returns:
            JSON with spawned actor's name and details
        """
        result = await send_command("spawn_actor", {
            "actor_class": actor_class,
            "name": name,
            "location": location or [0, 0, 0],
            "rotation": rotation or [0, 0, 0],
            "scale": scale or [1, 1, 1],
            "blueprint_path": blueprint_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_actor(actor_name: str) -> str:
        """Delete an actor from the current level.

        Args:
            actor_name: Name of the actor to delete (as shown in World Outliner)

        Returns:
            Deletion result
        """
        result = await send_command("delete_actor", {
            "actor_name": actor_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_actor_transform(
        actor_name: str,
        location: list[float] | None = None,
        rotation: list[float] | None = None,
        scale: list[float] | None = None,
    ) -> str:
        """Set the transform (position, rotation, scale) of an actor.

        Args:
            actor_name: Name of the target actor
            location: New world location [x, y, z] (None = don't change)
            rotation: New world rotation [pitch, yaw, roll] in degrees (None = don't change)
            scale: New world scale [x, y, z] (None = don't change)

        Returns:
            Updated transform details
        """
        params = {"actor_name": actor_name}
        if location is not None:
            params["location"] = location
        if rotation is not None:
            params["rotation"] = rotation
        if scale is not None:
            params["scale"] = scale

        result = await send_command("set_actor_transform", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_actors_in_level(
        class_filter: str = "",
        name_filter: str = "",
        tag_filter: str = "",
    ) -> str:
        """List all actors in the current level (World Outliner).

        Args:
            class_filter: Filter by class name (e.g., 'StaticMeshActor', empty = all)
            name_filter: Filter by actor name substring (empty = all)
            tag_filter: Filter by actor tag (empty = all)

        Returns:
            JSON array of actors with their names, classes, transforms, and tags
        """
        result = await send_command("get_actors_in_level", {
            "class_filter": class_filter,
            "name_filter": name_filter,
            "tag_filter": tag_filter,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def find_actors(query: str) -> str:
        """Search for actors by name in the current level.

        Args:
            query: Search query (actor name or partial name)

        Returns:
            JSON array of matching actors with details
        """
        result = await send_command("find_actors", {
            "query": query,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def duplicate_actor(
        actor_name: str,
        new_name: str = "",
        location_offset: list[float] | None = None,
    ) -> str:
        """Duplicate an actor in the current level.

        Args:
            actor_name: Name of the actor to duplicate
            new_name: Optional name for the duplicate
            location_offset: Offset from original position [x, y, z]

        Returns:
            Details of the duplicated actor
        """
        result = await send_command("duplicate_actor", {
            "actor_name": actor_name,
            "new_name": new_name,
            "location_offset": location_offset or [100, 0, 0],
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

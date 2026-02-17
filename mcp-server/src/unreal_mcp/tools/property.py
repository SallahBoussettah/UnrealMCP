"""Property inspection tools for Unreal Engine (Details Panel equivalent)."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_property_tools(mcp: FastMCP) -> None:
    """Register all property inspection MCP tools."""

    @mcp.tool()
    async def get_object_properties(
        object_path: str,
        category_filter: str = "",
        include_inherited: bool = True,
    ) -> str:
        """Get all properties of a UObject (actor, component, asset, etc.) - equivalent to the Details Panel.

        Args:
            object_path: Path to the object. For actors, use the actor name.
                For assets, use the asset path (e.g., '/Game/Blueprints/BP_MyActor.BP_MyActor').
            category_filter: Only show properties in this category (empty = all)
            include_inherited: Include properties inherited from parent classes

        Returns:
            JSON with all properties: name, type, value, category, metadata, editability
        """
        result = await send_command("get_object_properties", {
            "object_path": object_path,
            "category_filter": category_filter,
            "include_inherited": include_inherited,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_object_property(
        object_path: str,
        property_name: str,
        property_value: str,
    ) -> str:
        """Set a property value on a UObject (triggers PreEditChange/PostEditChange for proper editor behavior).

        Args:
            object_path: Path to the object (actor name or asset path)
            property_name: Name of the property to modify (supports dot-notation for nested properties,
                e.g., 'RelativeLocation.X')
            property_value: New value as string (will be parsed according to the property type)

        Returns:
            Result with old and new values
        """
        result = await send_command("set_object_property", {
            "object_path": object_path,
            "property_name": property_name,
            "property_value": property_value,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_component_hierarchy(actor_name: str) -> str:
        """Get the component hierarchy of an actor, showing all components and their properties.

        Args:
            actor_name: Name of the actor to inspect

        Returns:
            JSON tree of components with their types, properties, and child components
        """
        result = await send_command("get_component_hierarchy", {
            "actor_name": actor_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_class_defaults(class_name: str) -> str:
        """Get the Class Default Object (CDO) properties for a class.

        Args:
            class_name: Name of the class (e.g., 'StaticMeshActor', or a Blueprint path)

        Returns:
            JSON with all default property values for the class
        """
        result = await send_command("get_class_defaults", {
            "class_name": class_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_component_property(
        actor_name: str,
        component_name: str,
        property_name: str,
        property_value: str,
    ) -> str:
        """Set a property on a specific component of an actor.

        Args:
            actor_name: Name of the actor
            component_name: Name of the component
            property_name: Property to modify
            property_value: New value as string

        Returns:
            Result with old and new values
        """
        result = await send_command("set_component_property", {
            "actor_name": actor_name,
            "component_name": component_name,
            "property_name": property_name,
            "property_value": property_value,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

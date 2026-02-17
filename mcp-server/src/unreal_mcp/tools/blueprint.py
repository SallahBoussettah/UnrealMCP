"""Blueprint CRUD tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_blueprint_tools(mcp: FastMCP) -> None:
    """Register all blueprint-related MCP tools."""

    @mcp.tool()
    async def create_blueprint(
        name: str,
        parent_class: str = "Actor",
        path: str = "/Game/Blueprints",
    ) -> str:
        """Create a new Blueprint class in the Unreal Engine project.

        Args:
            name: Name for the new Blueprint (e.g., 'BP_MyActor')
            parent_class: Parent class to derive from. Common values:
                - 'Actor' (default)
                - 'Pawn'
                - 'Character'
                - 'GameModeBase'
                - 'PlayerController'
                - 'ActorComponent'
                - 'SceneComponent'
                - Or any custom class name
            path: Content folder path (e.g., '/Game/Blueprints')

        Returns:
            JSON with the created Blueprint's asset path and details
        """
        result = await send_command("create_blueprint", {
            "name": name,
            "parent_class": parent_class,
            "path": path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def list_blueprints(
        path: str = "/Game",
        recursive: bool = True,
    ) -> str:
        """List all Blueprint assets in a directory.

        Args:
            path: Content folder path to search (e.g., '/Game/Blueprints')
            recursive: Whether to search subdirectories

        Returns:
            JSON array of Blueprint asset paths and their parent classes
        """
        result = await send_command("list_blueprints", {
            "path": path,
            "recursive": recursive,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_blueprint_info(asset_path: str) -> str:
        """Get detailed information about a Blueprint including its variables, functions, components, and graphs.

        Args:
            asset_path: Full asset path (e.g., '/Game/Blueprints/BP_MyActor.BP_MyActor')

        Returns:
            JSON with Blueprint structure: variables, functions, components, event graphs, parent class
        """
        result = await send_command("get_blueprint_info", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def compile_blueprint(asset_path: str) -> str:
        """Compile a Blueprint and return detailed error/warning information.

        Returns status plus per-node error details including node_id, graph name,
        error message, severity, and node position. Use this to find and fix
        compilation issues programmatically.

        Args:
            asset_path: Full asset path of the Blueprint to compile

        Returns:
            JSON with: name, status (Error/UpToDate/Dirty), has_errors, error_count,
            warning_count, errors[] and warnings[] arrays. Each error/warning contains:
            node_id, node_title, node_class, graph, message, severity, pos_x, pos_y.
        """
        result = await send_command("compile_blueprint", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_blueprint(asset_path: str) -> str:
        """Delete a Blueprint asset.

        Args:
            asset_path: Full asset path of the Blueprint to delete

        Returns:
            Deletion result
        """
        result = await send_command("delete_blueprint", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_blueprint_variable(
        asset_path: str,
        variable_name: str,
        variable_type: str,
        default_value: str = "",
        is_instance_editable: bool = True,
        category: str = "Default",
    ) -> str:
        """Add a member variable to a Blueprint.

        Args:
            asset_path: Blueprint asset path
            variable_name: Name for the new variable
            variable_type: Type of the variable. Supported types:
                - 'Boolean', 'Byte', 'Integer', 'Integer64', 'Float', 'Double'
                - 'String', 'Text', 'Name'
                - 'Vector', 'Rotator', 'Transform'
                - 'Object' (requires specifying a class)
                - 'Class'
            default_value: Default value as string (optional)
            is_instance_editable: Whether the variable is editable per-instance in the details panel
            category: Category to organize the variable under in the details panel

        Returns:
            Result of the variable addition
        """
        result = await send_command("add_blueprint_variable", {
            "asset_path": asset_path,
            "variable_name": variable_name,
            "variable_type": variable_type,
            "default_value": default_value,
            "is_instance_editable": is_instance_editable,
            "category": category,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def remove_blueprint_variable(
        asset_path: str,
        variable_name: str,
    ) -> str:
        """Remove a member variable from a Blueprint.

        Args:
            asset_path: Blueprint asset path
            variable_name: Name of the variable to remove

        Returns:
            Result of the variable removal
        """
        result = await send_command("remove_blueprint_variable", {
            "asset_path": asset_path,
            "variable_name": variable_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_blueprint_component(
        asset_path: str,
        component_class: str,
        component_name: str,
        parent_component: str = "",
        location: list[float] | None = None,
        rotation: list[float] | None = None,
        scale: list[float] | None = None,
    ) -> str:
        """Add a component to a Blueprint's component hierarchy.

        Args:
            asset_path: Blueprint asset path
            component_class: Component class name (e.g., 'StaticMeshComponent', 'BoxCollisionComponent', 'PointLightComponent')
            component_name: Name for the new component
            parent_component: Parent component name to attach to (empty = root)
            location: Relative location [x, y, z]
            rotation: Relative rotation [pitch, yaw, roll]
            scale: Relative scale [x, y, z]

        Returns:
            Result of the component addition
        """
        result = await send_command("add_blueprint_component", {
            "asset_path": asset_path,
            "component_class": component_class,
            "component_name": component_name,
            "parent_component": parent_component,
            "location": location or [0, 0, 0],
            "rotation": rotation or [0, 0, 0],
            "scale": scale or [1, 1, 1],
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_blueprint_component_defaults(
        asset_path: str,
        component_name: str,
        property_name: str,
        property_value: str,
    ) -> str:
        """Set a default property value on a Blueprint's component template (CDO).

        This sets properties on SCS component templates so that every spawned
        instance of the Blueprint automatically inherits the value. Use this to
        set meshes, collision profiles, materials, and other defaults on Blueprint
        components — things that `set_object_property` and `set_component_property`
        cannot reach because SCS templates are not world actors.

        Args:
            asset_path: Blueprint asset path (e.g., '/Game/Blueprints/BP_MyActor.BP_MyActor')
            component_name: Name of the component as shown in the Blueprint editor
                (the variable name, e.g., 'VisualMesh', 'DamageBox')
            property_name: Property to set (e.g., 'StaticMesh', 'CollisionProfileName',
                'RelativeLocation', 'Intensity', 'LightColor')
            property_value: Value as a string in UE text format. Examples:
                - StaticMesh: "/Engine/BasicShapes/Cube.Cube"
                - CollisionProfileName: "OverlapAllDynamic"
                - RelativeLocation: "(X=0.0,Y=0.0,Z=50.0)"
                - Intensity: "5000.0"
                - LightColor: "(R=255,G=0,B=0,A=255)"

        Returns:
            JSON with the component name, property name, and value that was set
        """
        result = await send_command("set_blueprint_component_defaults", {
            "asset_path": asset_path,
            "component_name": component_name,
            "property_name": property_name,
            "property_value": property_value,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

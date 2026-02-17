"""Material editing tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_material_tools(mcp: FastMCP) -> None:
    """Register all material editing MCP tools."""

    @mcp.tool()
    async def create_material(
        name: str,
        path: str = "/Game/Materials",
        base_color: list[float] | None = None,
        roughness: float = 0.5,
        metallic: float = 0.0,
    ) -> str:
        """Create a new Material asset with initial property values.

        Args:
            name: Name for the material (e.g., 'M_Rock', 'M_Metal')
            path: Content folder path (default: '/Game/Materials')
            base_color: RGB color [r, g, b] with values 0.0-1.0 (default: [0.8, 0.8, 0.8])
            roughness: Roughness value 0.0-1.0 (default: 0.5)
            metallic: Metallic value 0.0-1.0 (default: 0.0)

        Returns:
            JSON with the created material's asset path
        """
        result = await send_command("create_material", {
            "name": name,
            "path": path,
            "base_color": base_color or [0.8, 0.8, 0.8],
            "roughness": roughness,
            "metallic": metallic,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def assign_material(
        actor_name: str,
        material_path: str,
        slot: int = 0,
        component_name: str = "",
    ) -> str:
        """Assign a material to a mesh component on an actor.

        Args:
            actor_name: Name of the target actor
            material_path: Asset path of the material (e.g., '/Game/Materials/M_Rock.M_Rock')
            slot: Material slot index (default: 0)
            component_name: Specific component name (empty = first mesh component)

        Returns:
            Assignment result with actor, component, material, and slot info
        """
        result = await send_command("assign_material", {
            "actor_name": actor_name,
            "material_path": material_path,
            "slot": slot,
            "component_name": component_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def modify_material(
        asset_path: str,
        base_color: list[float] | None = None,
        roughness: float | None = None,
        metallic: float | None = None,
        scalar_params: dict[str, float] | None = None,
        vector_params: dict[str, list[float]] | None = None,
    ) -> str:
        """Modify properties of an existing material or material instance.

        For base Materials: updates the constant expression values for base_color, roughness, metallic.
        For Material Instances: sets scalar/vector parameter overrides.

        Args:
            asset_path: Full asset path of the material
            base_color: New RGB base color [r, g, b] (base materials only)
            roughness: New roughness value (base materials or scalar param on instances)
            metallic: New metallic value (base materials or scalar param on instances)
            scalar_params: Dict of scalar parameter overrides (material instances).
                Example: {"Roughness": 0.3, "Metallic": 0.9}
            vector_params: Dict of vector parameter overrides (material instances).
                Example: {"BaseColor": [1.0, 0.0, 0.0, 1.0]}

        Returns:
            Modification result
        """
        params: dict = {"asset_path": asset_path}
        if base_color is not None:
            params["base_color"] = base_color
        if roughness is not None:
            params["roughness"] = roughness
        if metallic is not None:
            params["metallic"] = metallic
        if scalar_params:
            params["scalar_params"] = scalar_params
        if vector_params:
            params["vector_params"] = vector_params

        result = await send_command("modify_material", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_material_info(asset_path: str) -> str:
        """Get detailed information about a material including its parameters.

        Args:
            asset_path: Full asset path of the material (e.g., '/Game/Materials/M_Rock.M_Rock')

        Returns:
            JSON with material name, class, scalar parameters, vector parameters,
            expression count (for base materials), or parent material (for instances)
        """
        result = await send_command("get_material_info", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

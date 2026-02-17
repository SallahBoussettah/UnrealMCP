"""Viewport screenshot tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_viewport_tools(mcp: FastMCP) -> None:
    """Register all viewport-related MCP tools."""

    @mcp.tool()
    async def take_screenshot(
        width: int = 1280,
        height: int = 720,
    ) -> str:
        """Take a screenshot of the current editor viewport.

        Args:
            width: Screenshot width in pixels (default: 1280)
            height: Screenshot height in pixels (default: 720)

        Returns:
            Base64-encoded PNG image data
        """
        result = await send_command("take_screenshot", {
            "width": width,
            "height": height,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def focus_viewport(
        target: str = "",
        location: list[float] | None = None,
        rotation: list[float] | None = None,
        distance: float = 500.0,
    ) -> str:
        """Focus the editor viewport on a target actor or location.

        Args:
            target: Actor name to focus on (takes priority over location)
            location: World location to look at [x, y, z] (used if target is empty)
            rotation: Camera rotation [pitch, yaw, roll] (optional)
            distance: Distance from the target (default: 500)

        Returns:
            New viewport camera position and rotation
        """
        result = await send_command("focus_viewport", {
            "target": target,
            "location": location or [0, 0, 0],
            "rotation": rotation or [0, 0, 0],
            "distance": distance,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

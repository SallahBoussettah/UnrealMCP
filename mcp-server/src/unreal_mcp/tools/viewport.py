"""Viewport screenshot tools for Unreal Engine."""

import base64
import os
import time

from mcp.server.fastmcp import FastMCP

from ..connection import send_command

# Save screenshots next to the MCP server by default
SCREENSHOT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), "screenshots")


def register_viewport_tools(mcp: FastMCP) -> None:
    """Register all viewport-related MCP tools."""

    @mcp.tool()
    async def take_screenshot(
        width: int = 1280,
        height: int = 720,
        filename: str = "",
    ) -> str:
        """Take a screenshot of the current editor viewport and save it as a PNG file.

        Args:
            width: Screenshot width in pixels (default: 1280)
            height: Screenshot height in pixels (default: 720)
            filename: Custom filename (without extension). If empty, uses timestamp.

        Returns:
            Path to the saved PNG file
        """
        result = await send_command("take_screenshot", {
            "width": width,
            "height": height,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"

        data = result.get("data", {})
        b64_data = data.get("image_base64", "")
        if not b64_data:
            return "Error: No image data received from UE5"

        # Decode and save
        os.makedirs(SCREENSHOT_DIR, exist_ok=True)
        if not filename:
            filename = f"screenshot_{int(time.time())}"
        filepath = os.path.join(SCREENSHOT_DIR, f"{filename}.png")

        img_bytes = base64.b64decode(b64_data)
        with open(filepath, "wb") as f:
            f.write(img_bytes)

        return f"Screenshot saved: {filepath} ({len(img_bytes)} bytes, {width}x{height})"

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

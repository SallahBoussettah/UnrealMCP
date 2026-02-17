"""Play-in-Editor (PIE) control tools for Unreal Engine."""

import asyncio

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_pie_tools(mcp: FastMCP) -> None:
    """Register all PIE control MCP tools."""

    @mcp.tool()
    async def start_pie(mode: str = "") -> str:
        """Start a Play-in-Editor session to run the game inside the editor.

        Use this to test gameplay, Blueprints, and actor interactions at runtime.
        The existing get_console_logs tool captures all PIE runtime logs (PrintString,
        gameplay errors, etc.) automatically.

        Args:
            mode: How to launch PIE. Options:
                - '' (empty) - uses the editor's last PIE settings (default)
                - 'viewport' - play in the active level editor viewport
                - 'new_window' - play in a separate standalone window
                - 'simulate' - simulate without possessing a pawn (Simulate In Editor)

        Returns:
            JSON with is_running, is_simulating, mode, and world_name
        """
        params: dict = {}
        if mode:
            params["mode"] = mode

        result = await send_command("start_pie", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"

        data = result.get("data", {})

        # PIE world creation may be deferred to the next engine tick for
        # viewport/new_window modes. Poll briefly to return accurate status.
        if not data.get("is_running"):
            for _ in range(10):
                await asyncio.sleep(0.1)
                status = await send_command("get_pie_status", {})
                if status.get("success") and status.get("data", {}).get("is_running"):
                    data = status["data"]
                    data["mode"] = mode if mode else "viewport"
                    break

        return str(data)

    @mcp.tool()
    async def stop_pie() -> str:
        """Stop the current Play-in-Editor session.

        Returns:
            JSON confirming the stop was requested
        """
        result = await send_command("stop_pie", {})
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_pie_status() -> str:
        """Get the current Play-in-Editor session status.

        Returns:
            JSON with is_running, is_paused, is_simulating, world_name, and player_count
        """
        result = await send_command("get_pie_status", {})
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_pie_paused(paused: bool) -> str:
        """Pause or resume the current Play-in-Editor session.

        Args:
            paused: True to pause gameplay, False to resume

        Returns:
            JSON with the current is_paused and is_running state
        """
        result = await send_command("set_pie_paused", {"paused": paused})
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

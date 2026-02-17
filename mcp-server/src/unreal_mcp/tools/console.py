"""Console log reading tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_console_tools(mcp: FastMCP) -> None:
    """Register all console log reading MCP tools."""

    @mcp.tool()
    async def get_console_logs(
        count: int = 50,
        verbosity_filter: str = "",
        category_filter: str = "",
    ) -> str:
        """Get recent console log messages from the Unreal Editor.

        Args:
            count: Maximum number of log messages to return (default: 50)
            verbosity_filter: Filter by verbosity level. Options:
                - '' (empty) - all messages
                - 'Error' - errors only
                - 'Warning' - warnings and errors
                - 'Display' - display messages and above
                - 'Log' - log messages and above
            category_filter: Filter by log category (e.g., 'LogBlueprint', 'LogTemp', 'LogCompile')

        Returns:
            JSON array of log entries with timestamp, verbosity, category, and message
        """
        result = await send_command("get_console_logs", {
            "count": count,
            "verbosity_filter": verbosity_filter,
            "category_filter": category_filter,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def execute_console_command(command: str, target: str = "") -> str:
        """Execute a console command in the Unreal Editor or in a running PIE session.

        Args:
            command: Console command to execute (e.g., 'stat fps', 'obj list')
            target: Where to execute the command. Options:
                - '' (empty) - execute in the editor world (default)
                - 'editor' - execute in the editor world
                - 'pie' - execute in the PIE (Play-in-Editor) world.
                    Requires an active PIE session (start one with start_pie).

        Returns:
            Command execution result including target and status
        """
        params: dict = {"command": command}
        if target:
            params["target"] = target

        result = await send_command("execute_console_command", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def batch_execute(
        commands: list[dict],
        stop_on_error: bool = False,
    ) -> str:
        """Execute multiple commands in a single batch operation. Each command runs
        sequentially on the game thread in one TCP round-trip.

        Args:
            commands: List of command objects, each with 'command' (str) and 'params' (dict).
                Example: [
                    {"command": "spawn_actor", "params": {"actor_class": "PointLight", "location": [0, 0, 300]}},
                    {"command": "spawn_actor", "params": {"actor_class": "PointLight", "location": [200, 0, 300]}}
                ]
            stop_on_error: If True, stop executing remaining commands when one fails (default: False)

        Returns:
            JSON with results array (one entry per command), total count, and executed count
        """
        result = await send_command("batch_execute", {
            "commands": commands,
            "stop_on_error": stop_on_error,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

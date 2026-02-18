"""Blueprint debugging tools for Unreal Engine — breakpoints, stepping, watches."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_debug_tools(mcp: FastMCP) -> None:
    """Register all Blueprint debugging MCP tools."""

    @mcp.tool()
    async def set_breakpoint(
        asset_path: str,
        node_id: str,
        graph_name: str = "",
        enabled: bool = True,
    ) -> str:
        """Set or toggle a breakpoint on a Blueprint node.

        Breakpoints pause execution during PIE when the node is reached,
        allowing you to inspect variable values and step through logic.

        Args:
            asset_path: Blueprint asset path (e.g., '/Game/Blueprints/BP_MyActor.BP_MyActor')
            node_id: GUID of the node to set the breakpoint on (from get_graph_nodes)
            graph_name: Optional graph name to narrow the search
            enabled: True to enable the breakpoint, False to disable it

        Returns:
            JSON with node_id, node_title, enabled state, and validity
        """
        result = await send_command("set_breakpoint", {
            "asset_path": asset_path,
            "node_id": node_id,
            "graph_name": graph_name,
            "enabled": enabled,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_breakpoints(asset_path: str) -> str:
        """List all breakpoints in a Blueprint.

        Args:
            asset_path: Blueprint asset path

        Returns:
            JSON with breakpoints array (node_id, node_title, enabled, is_valid,
            graph_name, position) and count
        """
        result = await send_command("get_breakpoints", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_watch_values(asset_path: str = "") -> str:
        """Read watched pin values during a paused PIE session.

        Must be paused at a breakpoint. Returns the current values of all
        watched pins in the Blueprint's debugger.

        Args:
            asset_path: Blueprint asset path (optional — focuses on a specific BP)

        Returns:
            JSON with watches array (pin_name, pin_type, node_id, value, status),
            current_node_id, and count
        """
        result = await send_command("get_watch_values", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def step_execution(step_type: str) -> str:
        """Step Blueprint debugger execution during a paused PIE session.

        Args:
            step_type: Type of step to perform:
                - 'into' — step into the next node (follows function calls)
                - 'over' — step over to the next node in current graph
                - 'out' — step out of the current function/graph
                - 'resume' — resume normal execution (unpause)

        Returns:
            JSON with step_type and is_single_stepping state
        """
        result = await send_command("step_execution", {
            "step_type": step_type,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_call_stack() -> str:
        """Get the Blueprint execution trace/call stack when paused at a breakpoint.

        Returns the current instruction, most recent breakpoint hit, and a trace
        of recently executed nodes with their functions and timestamps.

        Returns:
            JSON with current_instruction, breakpoint_hit, trace_stack array
            (node_id, node_title, function, context, graph_name, time),
            frame_count, and is_single_stepping
        """
        result = await send_command("get_call_stack", {})
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

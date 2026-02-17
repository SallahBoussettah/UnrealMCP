"""Blueprint node graph editing tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_node_graph_tools(mcp: FastMCP) -> None:
    """Register all node graph editing MCP tools."""

    @mcp.tool()
    async def add_node(
        asset_path: str,
        graph_name: str = "EventGraph",
        node_type: str = "CallFunction",
        function_name: str = "",
        target_class: str = "",
        node_position: list[float] | None = None,
        params: dict | None = None,
    ) -> str:
        """Add a node to a Blueprint graph.

        Args:
            asset_path: Blueprint asset path
            graph_name: Name of the graph ('EventGraph', function name, or macro name)
            node_type: Type of node to create. Options:
                - 'CallFunction' - Call a UFUNCTION (requires function_name and target_class)
                - 'Event' - Event node (BeginPlay, Tick, etc.)
                - 'CustomEvent' - Custom event definition
                - 'Branch' - If/else branch
                - 'Sequence' - Execution sequence
                - 'VariableGet' - Get variable value
                - 'VariableSet' - Set variable value
                - 'Cast' - Dynamic cast
                - 'SpawnActor' - Spawn actor from class
                - 'ForEachLoop' - For each loop
                - 'WhileLoop' - While loop
                - 'MakeArray' - Make array
                - 'PrintString' - Print string (debug)
                - 'Delay' - Delay node
                - 'Timeline' - Timeline node
                - 'Self' - Self reference
                - 'MakeStruct' - Make struct
                - 'BreakStruct' - Break struct
            function_name: For CallFunction nodes, the function name
            target_class: For CallFunction nodes, the class that owns the function
            node_position: [x, y] position in the graph editor
            params: Additional node-specific parameters (e.g., variable_name for Get/Set, event_name for Event)

        Returns:
            JSON with created node ID and pin information
        """
        result = await send_command("add_node", {
            "asset_path": asset_path,
            "graph_name": graph_name,
            "node_type": node_type,
            "function_name": function_name,
            "target_class": target_class,
            "node_position": node_position or [0, 0],
            "params": params or {},
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def connect_pins(
        asset_path: str,
        source_node_id: str,
        source_pin_name: str,
        target_node_id: str,
        target_pin_name: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Connect two pins between nodes in a Blueprint graph.

        Args:
            asset_path: Blueprint asset path
            source_node_id: ID of the source node (output side)
            source_pin_name: Name of the output pin on the source node
            target_node_id: ID of the target node (input side)
            target_pin_name: Name of the input pin on the target node
            graph_name: Name of the graph containing the nodes

        Returns:
            Connection result
        """
        result = await send_command("connect_pins", {
            "asset_path": asset_path,
            "source_node_id": source_node_id,
            "source_pin_name": source_pin_name,
            "target_node_id": target_node_id,
            "target_pin_name": target_pin_name,
            "graph_name": graph_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def disconnect_pins(
        asset_path: str,
        node_id: str,
        pin_name: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Disconnect all connections from a specific pin.

        Args:
            asset_path: Blueprint asset path
            node_id: ID of the node containing the pin
            pin_name: Name of the pin to disconnect
            graph_name: Name of the graph

        Returns:
            Disconnection result
        """
        result = await send_command("disconnect_pins", {
            "asset_path": asset_path,
            "node_id": node_id,
            "pin_name": pin_name,
            "graph_name": graph_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_node(
        asset_path: str,
        node_id: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Delete a node from a Blueprint graph.

        Args:
            asset_path: Blueprint asset path
            node_id: ID of the node to delete
            graph_name: Name of the graph

        Returns:
            Deletion result
        """
        result = await send_command("delete_node", {
            "asset_path": asset_path,
            "node_id": node_id,
            "graph_name": graph_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_graph_nodes(
        asset_path: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Get all nodes in a Blueprint graph with their pins and connections.

        Args:
            asset_path: Blueprint asset path
            graph_name: Name of the graph to inspect

        Returns:
            JSON array of all nodes with their IDs, types, positions, pins, and connections
        """
        result = await send_command("get_graph_nodes", {
            "asset_path": asset_path,
            "graph_name": graph_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_pin_value(
        asset_path: str,
        node_id: str,
        pin_name: str,
        value: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Set the default value of a pin on a node.

        Args:
            asset_path: Blueprint asset path
            node_id: ID of the node
            pin_name: Name of the pin
            value: Value to set (as string - will be parsed according to pin type)
            graph_name: Name of the graph

        Returns:
            Result of setting the pin value
        """
        result = await send_command("set_pin_value", {
            "asset_path": asset_path,
            "node_id": node_id,
            "pin_name": pin_name,
            "value": value,
            "graph_name": graph_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def create_function(
        asset_path: str,
        function_name: str,
        inputs: list[dict] | None = None,
        outputs: list[dict] | None = None,
    ) -> str:
        """Create a new function in a Blueprint.

        Args:
            asset_path: Blueprint asset path
            function_name: Name for the new function
            inputs: List of input parameters, each with 'name' and 'type' keys.
                Example: [{"name": "Health", "type": "Float"}, {"name": "Name", "type": "String"}]
            outputs: List of output parameters, each with 'name' and 'type' keys.
                Example: [{"name": "Success", "type": "Boolean"}]

        Returns:
            Result with the function's graph name and entry/exit node IDs
        """
        result = await send_command("create_function", {
            "asset_path": asset_path,
            "function_name": function_name,
            "inputs": inputs or [],
            "outputs": outputs or [],
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_function(
        asset_path: str,
        function_name: str,
    ) -> str:
        """Delete a function from a Blueprint.

        Args:
            asset_path: Blueprint asset path
            function_name: Name of the function to delete

        Returns:
            Deletion result
        """
        result = await send_command("delete_function", {
            "asset_path": asset_path,
            "function_name": function_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

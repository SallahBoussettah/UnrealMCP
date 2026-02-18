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
        """Add a node to a Blueprint graph. Supports 43 node types.

        Args:
            asset_path: Blueprint asset path (e.g. '/Game/Blueprints/BP_MyActor')
            graph_name: Name of the graph ('EventGraph', function name, or macro name)
            node_type: Type of node to create. See full list below.
            function_name: For CallFunction/CommutativeAssociativeBinaryOperator, the function name
            target_class: For CallFunction/CommutativeAssociativeBinaryOperator, the owning class
            node_position: [x, y] position in the graph editor
            params: Additional node-specific parameters (varies by node_type)

        Node types and their params:

        === FUNCTIONS & EVENTS (4) ===
        - 'CallFunction' - Call any UFUNCTION. Requires function_name and target_class.
            Examples: PrintString (KismetSystemLibrary), Delay (KismetSystemLibrary),
            GetActorLocation (Actor), SpawnSound2D (GameplayStatics)
        - 'Event' - Built-in event. params: {"event_name": "ReceiveBeginPlay"}
            Common events: ReceiveBeginPlay, ReceiveTick, ReceiveDestroyed, ReceiveHit
        - 'CustomEvent' - Custom event. params: {"event_name": "MyEvent"}
        - 'Self' - Self reference node (no params needed)

        === VARIABLES (2) ===
        - 'VariableGet' - Get variable. params: {"variable_name": "Health"}
        - 'VariableSet' - Set variable. params: {"variable_name": "Health"}

        === FLOW CONTROL (7) ===
        - 'Branch' - If/else (no params needed)
        - 'Sequence' - Execution sequence (no params needed)
        - 'MultiGate' - Multiple execution outputs (no params needed)
        - 'Select' - Select value by index (no params needed)
        - 'DoOnceMultiInput' - Multi-input do once (no params needed)
        - 'MacroInstance' - Standard macro. params: {"macro_name": "ForLoop"}
            Available macros: ForLoop, ForLoopWithBreak, WhileLoop, DoOnce, DoN,
            Gate, FlipFlop, ForEachLoop, ForEachLoopWithBreak, IsValid
            Optional: {"macro_path": "/Game/MyMacroLibrary"} for custom macros
        - 'ForEachElementInEnum' - Loop enum values. params: {"enum_name": "ECollisionChannel"}

        === SWITCH (4) ===
        - 'SwitchInteger' - Switch on int (no params needed)
        - 'SwitchString' - Switch on string (no params needed)
        - 'SwitchName' - Switch on FName (no params needed)
        - 'SwitchEnum' - Switch on enum. params: {"enum_name": "ECollisionChannel"}

        === CASTING (2) ===
        - 'DynamicCast' - Cast To. params: {"target_class": "Character"}
        - 'ClassDynamicCast' - Class cast. params: {"target_class": "Character"}

        === STRUCTS (3) ===
        - 'MakeStruct' - Make struct. params: {"struct_type": "Vector"}
            Common structs: Vector, Rotator, Transform, LinearColor, Vector2D, HitResult
        - 'BreakStruct' - Break struct. params: {"struct_type": "Vector"}
        - 'SetFieldsInStruct' - Set struct fields. params: {"struct_type": "Vector"}

        === CONTAINERS (4) ===
        - 'MakeArray' - Make array. Optional params: {"num_inputs": 3}
        - 'MakeMap' - Make map (no params needed)
        - 'MakeSet' - Make set (no params needed)
        - 'GetArrayItem' - Get array element by index (no params needed)

        === SPAWNING & OBJECTS (3) ===
        - 'SpawnActorFromClass' - Spawn Actor from Class (set class via pin)
        - 'GenericCreateObject' - Construct Object from Class (set class via pin)
        - 'AddComponentByClass' - Add Component by Class (set class via pin)

        === DELEGATES (5) ===
        - 'CreateDelegate' - Create delegate binding (no params needed)
        - 'AddDelegate' - Bind to event dispatcher. params: {"delegate_name": "OnDamage"}
        - 'RemoveDelegate' - Unbind from dispatcher. params: {"delegate_name": "OnDamage"}
        - 'CallDelegate' - Fire event dispatcher. params: {"delegate_name": "OnDamage"}
        - 'ClearDelegate' - Clear all bindings. params: {"delegate_name": "OnDamage"}

        === TEXT & ENUMS (2) ===
        - 'FormatText' - Format Text with wildcards (no params needed)
        - 'EnumLiteral' - Enum value. params: {"enum_name": "ECollisionChannel"}

        === MISC (7) ===
        - 'Timeline' - Timeline node. params: {"timeline_name": "MyTimeline"}
        - 'Knot' - Reroute node (no params needed)
        - 'LoadAsset' - Async load asset (no params needed)
        - 'EaseFunction' - Ease/interpolation (no params needed)
        - 'GetClassDefaults' - Get Class Defaults (set class via pin)
        - 'GetDataTableRow' - Data table lookup (set table via pin)
        - 'CommutativeAssociativeBinaryOperator' - Expandable math op.
            Requires function_name and target_class, like CallFunction.
            Example: function_name='Add_FloatFloat', target_class='KismetMathLibrary'

        Returns:
            JSON with created node ID, class, title, position, and pin information
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

    @mcp.tool()
    async def arrange_graph(
        asset_path: str,
        graph_name: str = "EventGraph",
        horizontal_spacing: int = 350,
        vertical_spacing: int = 200,
        subgraph_spacing: int = 400,
    ) -> str:
        """Auto-layout all nodes in a Blueprint graph using a layered graph algorithm.

        Arranges exec-flow nodes left-to-right in layers (BFS from roots), then
        places data-only nodes (VariableGet, Self, etc.) near their consumers.
        Disconnected subgraphs are stacked vertically.

        Args:
            asset_path: Blueprint asset path (e.g. '/Game/Blueprints/BP_MyActor')
            graph_name: Name of the graph to arrange (default: 'EventGraph')
            horizontal_spacing: Pixels between layers/columns (default: 350)
            vertical_spacing: Pixels between nodes in the same layer (default: 200)
            subgraph_spacing: Pixels between disconnected subgraphs (default: 400)

        Returns:
            JSON with count of arranged nodes, subgraphs found, and new positions
        """
        result = await send_command("arrange_nodes", {
            "asset_path": asset_path,
            "graph_name": graph_name,
            "horizontal_spacing": horizontal_spacing,
            "vertical_spacing": vertical_spacing,
            "subgraph_spacing": subgraph_spacing,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

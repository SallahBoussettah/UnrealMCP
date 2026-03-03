"""High-level batch tools that reduce MCP round-trips for common Blueprint workflows."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_batch_tools(mcp: FastMCP) -> None:
    """Register high-level batch MCP tools."""

    @mcp.tool()
    async def batch_add_nodes(
        asset_path: str,
        nodes: list[dict],
        graph_name: str = "EventGraph",
        stop_on_error: bool = True,
    ) -> str:
        """Add multiple nodes to a Blueprint graph in one call.

        Args:
            asset_path: Blueprint asset path (e.g. '/Game/Blueprints/BP_MyActor')
            nodes: List of node definitions. Each dict can have:
                - node_type (str): Node type (default: 'CallFunction'). See add_node for full list.
                - function_name (str): For CallFunction nodes
                - target_class (str): For CallFunction nodes
                - node_position (list[float]): [x, y] position
                - params (dict): Node-specific params (e.g. {"event_name": "ReceiveBeginPlay"})
            graph_name: Target graph name (default: 'EventGraph')
            stop_on_error: Stop on first failure (default: True)

        Returns:
            JSON with node_ids list (None for failed nodes), total, executed, and per-node results.
            Use the node_ids with batch_pin_operations to wire them up.

        Example:
            batch_add_nodes("/Game/BP_Test", [
                {"node_type": "Event", "params": {"event_name": "ReceiveBeginPlay"}, "node_position": [0, 0]},
                {"node_type": "CallFunction", "function_name": "PrintString", "target_class": "KismetSystemLibrary", "node_position": [300, 0]},
                {"node_type": "CallFunction", "function_name": "Delay", "target_class": "KismetSystemLibrary", "node_position": [600, 0]},
            ])
        """
        commands = []
        for i, node in enumerate(nodes):
            commands.append({
                "command": "add_node",
                "params": {
                    "asset_path": asset_path,
                    "graph_name": graph_name,
                    "node_type": node.get("node_type", "CallFunction"),
                    "function_name": node.get("function_name", ""),
                    "target_class": node.get("target_class", ""),
                    "node_position": node.get("node_position") or [i * 350, 0],
                    "params": node.get("params", {}),
                },
            })

        result = await send_command("batch_execute", {
            "commands": commands,
            "stop_on_error": stop_on_error,
        })

        data = result.get("data", {})
        if not data:
            # No data at all — a transport or protocol error
            return f"Error: {result.get('error', 'Unknown error')}"

        results = data.get("results", [])

        # Extract node_ids from each sub-result
        node_ids = []
        errors = []
        for r in results:
            if r.get("success") and r.get("data"):
                node_ids.append(r["data"].get("node_id"))
            else:
                node_ids.append(None)
                idx = r.get("index", "?")
                cmd = r.get("command", "?")
                err = r.get("error", "unknown")
                errors.append(f"[{idx}] {cmd}: {err}")

        output = {
            "node_ids": node_ids,
            "total": data.get("total", len(commands)),
            "executed": data.get("executed", 0),
            "results": results,
        }
        if errors:
            output["errors"] = errors
            output["error_summary"] = f"{len(errors)} of {len(commands)} nodes failed"

        return str(output)

    @mcp.tool()
    async def batch_pin_operations(
        asset_path: str,
        connections: list[dict] | None = None,
        pin_values: list[dict] | None = None,
        graph_name: str = "EventGraph",
        stop_on_error: bool = False,
    ) -> str:
        """Connect pins and set pin values in one call. Connections are processed first, then pin values.

        Args:
            asset_path: Blueprint asset path
            connections: List of pin connections. Each dict has:
                - source_node_id (str): Output node ID
                - source_pin (str): Output pin name
                - target_node_id (str): Input node ID
                - target_pin (str): Input pin name
            pin_values: List of pin value assignments. Each dict has:
                - node_id (str): Node ID
                - pin_name (str): Pin name
                - value (str): Value to set (as string)
            graph_name: Target graph name (default: 'EventGraph')
            stop_on_error: Stop on first failure (default: False)

        Returns:
            JSON with total operations, executed count, and per-operation results.

        Example:
            batch_pin_operations("/Game/BP_Test",
                connections=[
                    {"source_node_id": "N1", "source_pin": "then", "target_node_id": "N2", "target_pin": "execute"},
                    {"source_node_id": "N2", "source_pin": "ReturnValue", "target_node_id": "N3", "target_pin": "Object"},
                ],
                pin_values=[
                    {"node_id": "N2", "pin_name": "InString", "value": "Hello World"},
                    {"node_id": "N3", "pin_name": "Duration", "value": "2.0"},
                ],
            )
        """
        commands = []

        for conn in (connections or []):
            commands.append({
                "command": "connect_pins",
                "params": {
                    "asset_path": asset_path,
                    "graph_name": graph_name,
                    "source_node_id": conn["source_node_id"],
                    "source_pin_name": conn["source_pin"],
                    "target_node_id": conn["target_node_id"],
                    "target_pin_name": conn["target_pin"],
                },
            })

        for pv in (pin_values or []):
            commands.append({
                "command": "set_pin_value",
                "params": {
                    "asset_path": asset_path,
                    "graph_name": graph_name,
                    "node_id": pv["node_id"],
                    "pin_name": pv["pin_name"],
                    "value": pv["value"],
                },
            })

        if not commands:
            return str({"total": 0, "executed": 0, "results": []})

        result = await send_command("batch_execute", {
            "commands": commands,
            "stop_on_error": stop_on_error,
        })

        data = result.get("data", {})
        if not data:
            return f"Error: {result.get('error', 'Unknown error')}"

        # Surface per-operation errors if any failed
        if not result.get("success"):
            results = data.get("results", [])
            errors = []
            for r in results:
                if not r.get("success"):
                    idx = r.get("index", "?")
                    cmd = r.get("command", "?")
                    err = r.get("error", "unknown")
                    errors.append(f"[{idx}] {cmd}: {err}")
            if errors:
                data["errors"] = errors
                data["error_summary"] = f"{len(errors)} operations failed"

        return str(data)

    @mcp.tool()
    async def batch_spawn_actors(
        actors: list[dict],
        stop_on_error: bool = False,
    ) -> str:
        """Spawn multiple actors in the current level in one call.

        Args:
            actors: List of actor definitions. Each dict can have:
                - actor_class (str): Class name (default: 'StaticMeshActor'). Ignored if blueprint_path is set.
                - name (str): Optional actor label
                - location (list[float]): [x, y, z] world position (default: [0, 0, 0])
                - rotation (list[float]): [pitch, yaw, roll] in degrees (default: [0, 0, 0])
                - scale (list[float]): [x, y, z] scale (default: [1, 1, 1])
                - blueprint_path (str): Spawn a Blueprint instance instead of actor_class
            stop_on_error: Stop on first failure (default: False)

        Returns:
            JSON with spawned_actors list, total, and executed count.

        Example:
            batch_spawn_actors([
                {"blueprint_path": "/Game/BP_Door", "name": "Door1", "location": [0, 0, 0]},
                {"blueprint_path": "/Game/BP_Door", "name": "Door2", "location": [500, 0, 0]},
                {"actor_class": "PointLight", "location": [250, 0, 300]},
            ])
        """
        commands = []
        for actor in actors:
            commands.append({
                "command": "spawn_actor",
                "params": {
                    "actor_class": actor.get("actor_class", "StaticMeshActor"),
                    "name": actor.get("name", ""),
                    "location": actor.get("location", [0, 0, 0]),
                    "rotation": actor.get("rotation", [0, 0, 0]),
                    "scale": actor.get("scale", [1, 1, 1]),
                    "blueprint_path": actor.get("blueprint_path", ""),
                },
            })

        result = await send_command("batch_execute", {
            "commands": commands,
            "stop_on_error": stop_on_error,
        })

        data = result.get("data", {})
        if not data:
            return f"Error: {result.get('error', 'Unknown error')}"

        results = data.get("results", [])

        # Extract spawned actor info from each sub-result
        spawned_actors = []
        errors = []
        for r in results:
            if r.get("success") and r.get("data"):
                spawned_actors.append({
                    "name": r["data"].get("name", ""),
                    "class": r["data"].get("class", ""),
                })
            else:
                spawned_actors.append(None)
                idx = r.get("index", "?")
                cmd = r.get("command", "?")
                err = r.get("error", "unknown")
                errors.append(f"[{idx}] {cmd}: {err}")

        output = {
            "spawned_actors": spawned_actors,
            "total": data.get("total", len(commands)),
            "executed": data.get("executed", 0),
            "results": results,
        }
        if errors:
            output["errors"] = errors
            output["error_summary"] = f"{len(errors)} of {len(commands)} actors failed"

        return str(output)

"""Enhanced Input tools for Unreal Engine — create InputActions, InputMappingContexts, and configure key mappings."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_input_tools(mcp: FastMCP) -> None:
    """Register all Enhanced Input MCP tools."""

    @mcp.tool()
    async def create_input_action(
        asset_name: str,
        package_path: str = "/Game/Input",
        value_type: str = "Boolean",
        consume_input: bool = True,
        trigger_when_paused: bool = False,
    ) -> str:
        """Create a new Enhanced Input Action asset.

        InputActions define what player actions exist (Jump, Move, Look, etc.)
        and what type of value they produce.

        Args:
            asset_name: Name for the asset (e.g., 'IA_Jump', 'IA_Move')
            package_path: Content Browser path (default '/Game/Input')
            value_type: Value type — 'Boolean' (button), 'Axis1D' (trigger),
                        'Axis2D' (stick/WASD), or 'Axis3D' (default 'Boolean')
            consume_input: Whether this action consumes input (default True)
            trigger_when_paused: Whether this action triggers when game is paused (default False)

        Returns:
            JSON with asset_path, asset_name, value_type, consume_input, trigger_when_paused
        """
        result = await send_command("create_input_action", {
            "asset_name": asset_name,
            "package_path": package_path,
            "value_type": value_type,
            "consume_input": consume_input,
            "trigger_when_paused": trigger_when_paused,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def create_input_mapping_context(
        asset_name: str,
        package_path: str = "/Game/Input",
    ) -> str:
        """Create a new Input Mapping Context asset.

        InputMappingContexts bind physical keys/buttons to InputActions.
        After creation, use add_input_mapping to add key-to-action bindings.

        Args:
            asset_name: Name for the asset (e.g., 'IMC_Default', 'IMC_Vehicle')
            package_path: Content Browser path (default '/Game/Input')

        Returns:
            JSON with asset_path, asset_name, mappings_count (starts at 0)
        """
        result = await send_command("create_input_mapping_context", {
            "asset_name": asset_name,
            "package_path": package_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_input_mapping(
        context_path: str,
        action_path: str,
        key: str,
        modifiers: list[dict] | None = None,
        triggers: list[dict] | None = None,
    ) -> str:
        """Add a key binding to an Input Mapping Context.

        Maps a physical key to an InputAction, optionally with modifiers and triggers.
        Multiple keys can map to the same action (e.g., WASD all map to IA_Move).

        Common key names: SpaceBar, W, A, S, D, LeftMouseButton, RightMouseButton,
        Gamepad_LeftStick_X, Gamepad_RightStick_Y, Gamepad_FaceButton_Bottom, etc.

        Args:
            context_path: Asset path to the InputMappingContext
            action_path: Asset path to the InputAction
            key: FKey name string (e.g., 'SpaceBar', 'W', 'Gamepad_LeftStick_X')
            modifiers: Optional list of modifier descriptors:
                - {"type": "Negate", "bX": true, "bY": true, "bZ": true}
                - {"type": "SwizzleAxis", "order": "YXZ"} (also: ZYX, XZY, YZX, ZXY)
                - {"type": "DeadZone", "lower_threshold": 0.2, "upper_threshold": 1.0, "dead_zone_type": "Axial"}
                - {"type": "Scalar", "x": 2.0, "y": 1.0, "z": 1.0}
            triggers: Optional list of trigger descriptors:
                - {"type": "Down"} — fires while key is held
                - {"type": "Pressed"} — fires once on key press
                - {"type": "Released"} — fires once on key release
                - {"type": "Hold", "hold_time_threshold": 0.5, "is_one_shot": false}
                - {"type": "Tap", "tap_release_time_threshold": 0.2}

        Returns:
            JSON with context_path, action, key, modifiers_count, triggers_count, total_mappings

        Example — WASD movement for Axis2D IA_Move:
            W → SwizzleAxis(YXZ) — maps to +Y
            S → Negate(bY=true) + SwizzleAxis(YXZ) — maps to -Y
            A → Negate(bX=true) — maps to -X
            D → (no modifiers) — maps to +X
        """
        params: dict = {
            "context_path": context_path,
            "action_path": action_path,
            "key": key,
        }
        if modifiers is not None:
            params["modifiers"] = modifiers
        if triggers is not None:
            params["triggers"] = triggers
        result = await send_command("add_input_mapping", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def remove_input_mapping(
        context_path: str,
        action_path: str = "",
        key: str = "",
    ) -> str:
        """Remove a key binding from an Input Mapping Context.

        Can remove by specific key+action combo, all keys for an action,
        or all actions for a key.

        Args:
            context_path: Asset path to the InputMappingContext
            action_path: Asset path to the InputAction (optional if key is provided)
            key: FKey name string (optional if action_path is provided).
                 At least one of action_path or key must be provided.
                 - Both provided: removes that specific key+action mapping
                 - Only action_path: removes ALL key mappings for that action
                 - Only key: removes ALL action mappings for that key

        Returns:
            JSON with removed_count and remaining_mappings
        """
        result = await send_command("remove_input_mapping", {
            "context_path": context_path,
            "action_path": action_path,
            "key": key,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_input_mapping_context(
        context_path: str,
    ) -> str:
        """Read all mappings from an Input Mapping Context.

        Returns every key-to-action binding with its modifiers and triggers.

        Args:
            context_path: Asset path to the InputMappingContext

        Returns:
            JSON with context_name, total_mappings, and mappings array where each
            entry has: action, action_path, value_type, key, modifiers [], triggers []
        """
        result = await send_command("get_input_mapping_context", {
            "context_path": context_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

"""Animation Blueprint tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_anim_tools(mcp: FastMCP) -> None:
    """Register all animation blueprint MCP tools."""

    @mcp.tool()
    async def create_anim_blueprint(
        name: str,
        skeleton_path: str = "",
        skeletal_mesh_path: str = "",
        path: str = "/Game/Animations",
    ) -> str:
        """Create a new Animation Blueprint with a default state machine.

        Provide either skeleton_path or skeletal_mesh_path. If a skeletal mesh
        path is given, the skeleton is extracted from it automatically.

        Args:
            name: Name for the AnimBP (e.g., 'ABP_Character', 'ABP_Enemy')
            skeleton_path: Path to a USkeleton asset (e.g., '/Game/Characters/SK_Mannequin')
            skeletal_mesh_path: Path to a USkeletalMesh to derive the skeleton from
                (e.g., '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
            path: Content folder path (default: '/Game/Animations')

        Returns:
            JSON with name, asset_path, skeleton, and has_state_machine
        """
        result = await send_command("create_anim_blueprint", {
            "name": name,
            "path": path,
            "skeleton_path": skeleton_path,
            "skeletal_mesh_path": skeletal_mesh_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_anim_state(
        asset_path: str,
        state_name: str,
        animation_asset: str = "",
        state_machine_name: str = "",
        position: list[int] | None = None,
    ) -> str:
        """Add a state to an Animation Blueprint's state machine.

        Args:
            asset_path: Asset path of the AnimBP (e.g., '/Game/Animations/ABP_Character')
            state_name: Name for the new state (e.g., 'Idle', 'Walk', 'Run', 'Jump')
            animation_asset: Optional path to an AnimSequence or BlendSpace to play
                in this state (e.g., '/Game/Animations/Idle_Anim')
            state_machine_name: Name of a specific state machine (empty = first found)
            position: Optional [x, y] position in the graph. Auto-positioned if omitted.

        Returns:
            JSON with state_name, pos_x, pos_y, and animation info
        """
        params: dict = {
            "asset_path": asset_path,
            "state_name": state_name,
            "animation_asset": animation_asset,
            "state_machine_name": state_machine_name,
        }
        if position is not None:
            params["position"] = position

        result = await send_command("add_anim_state", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_anim_transition(
        asset_path: str,
        source_state: str,
        target_state: str,
        duration: float = 0.2,
        blend_mode: str = "Linear",
        state_machine_name: str = "",
    ) -> str:
        """Add a transition between two states in an AnimBP state machine.

        Args:
            asset_path: Asset path of the AnimBP
            source_state: Name of the source state
            target_state: Name of the target state
            duration: Crossfade duration in seconds (default: 0.2)
            blend_mode: Blend curve type. Options: Linear, HermiteCubic, Sinusoidal,
                QuadraticInOut, CubicInOut, QuarticInOut, QuinticInOut,
                CircularIn, CircularOut, CircularInOut, ExpIn, ExpOut, ExpInOut
            state_machine_name: Name of a specific state machine (empty = first found)

        Returns:
            JSON with source_state, target_state, duration, and blend_mode
        """
        result = await send_command("add_anim_transition", {
            "asset_path": asset_path,
            "source_state": source_state,
            "target_state": target_state,
            "duration": duration,
            "blend_mode": blend_mode,
            "state_machine_name": state_machine_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_anim_transition_rule(
        asset_path: str,
        source_state: str,
        target_state: str,
        rule_type: str,
        rule_params: dict | None = None,
        state_machine_name: str = "",
    ) -> str:
        """Set the transition condition for a state machine transition.

        The transition must already exist (use add_anim_transition first).

        Args:
            asset_path: Asset path of the AnimBP
            source_state: Name of the source state
            target_state: Name of the target state
            rule_type: Type of transition rule:
                - "auto_rule": Automatically transition when the state's animation finishes
                - "time_remaining": Transition when remaining time < threshold
                - "bool_variable": Transition when a bool variable on the AnimInstance is true
            rule_params: Parameters for the rule type:
                - For "time_remaining": {"threshold": 0.25} (seconds remaining)
                - For "bool_variable": {"variable": "bIsMoving"} (variable name on AnimInstance)
                - For "auto_rule": not needed
            state_machine_name: Name of a specific state machine (empty = first found)

        Returns:
            JSON with source_state, target_state, rule_type, and rule_info
        """
        params: dict = {
            "asset_path": asset_path,
            "source_state": source_state,
            "target_state": target_state,
            "rule_type": rule_type,
            "state_machine_name": state_machine_name,
        }
        if rule_params is not None:
            params["rule_params"] = rule_params

        result = await send_command("set_anim_transition_rule", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_blend_space(
        name: str,
        skeleton_path: str,
        type: str = "1D",
        axis_x_name: str = "Speed",
        axis_x_range: list[float] | None = None,
        axis_y_name: str = "Direction",
        axis_y_range: list[float] | None = None,
        samples: list[dict] | None = None,
        path: str = "/Game/Animations",
    ) -> str:
        """Create a BlendSpace1D or BlendSpace2D asset.

        Args:
            name: Name for the blend space (e.g., 'BS_Locomotion', 'BS_AimOffset')
            skeleton_path: Path to a USkeleton or USkeletalMesh asset
            type: "1D" for BlendSpace1D or "2D" for BlendSpace (default: "1D")
            axis_x_name: Display name for the X axis (default: "Speed")
            axis_x_range: [min, max] range for X axis (default: [0, 100])
            axis_y_name: Display name for the Y axis, 2D only (default: "Direction")
            axis_y_range: [min, max] range for Y axis, 2D only (default: [0, 100])
            samples: List of animation samples, each a dict with:
                {"animation": "/Game/Anims/Walk", "x": 150.0, "y": 0.0}
            path: Content folder path (default: '/Game/Animations')

        Returns:
            JSON with name, asset_path, type, skeleton, and samples_added count
        """
        params: dict = {
            "name": name,
            "path": path,
            "skeleton_path": skeleton_path,
            "type": type,
            "axis_x_name": axis_x_name,
            "axis_y_name": axis_y_name,
        }
        if axis_x_range is not None:
            params["axis_x_range"] = axis_x_range
        if axis_y_range is not None:
            params["axis_y_range"] = axis_y_range
        if samples is not None:
            params["samples"] = samples

        result = await send_command("add_blend_space", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_anim_montage(
        name: str,
        animation_path: str,
        skeleton_path: str = "",
        slot_name: str = "DefaultSlot",
        sections: list[dict] | None = None,
        path: str = "/Game/Animations",
    ) -> str:
        """Create an Animation Montage from an animation sequence.

        Args:
            name: Name for the montage (e.g., 'AM_Attack', 'AM_Dodge')
            animation_path: Path to the source AnimSequence
                (e.g., '/Game/Animations/Attack_Anim')
            skeleton_path: Optional path to a USkeleton or USkeletalMesh.
                Derived from the animation if not provided.
            slot_name: Montage slot name (default: 'DefaultSlot').
                Common slots: DefaultSlot, UpperBody, FullBody
            sections: Optional list of montage sections, each a dict with:
                {"name": "WindUp", "start_time": 0.0}
            path: Content folder path (default: '/Game/Animations')

        Returns:
            JSON with name, asset_path, skeleton, source_animation, slot_name,
            and sections_added count
        """
        params: dict = {
            "name": name,
            "path": path,
            "skeleton_path": skeleton_path,
            "animation_path": animation_path,
            "slot_name": slot_name,
        }
        if sections is not None:
            params["sections"] = sections

        result = await send_command("add_anim_montage", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_anim_graph(asset_path: str) -> str:
        """Get the full animation graph structure of an Animation Blueprint.

        Returns the complete state machine hierarchy including all states
        (with their animations and positions), transitions (with targets,
        durations, and rule types), and entry state information.

        Args:
            asset_path: Asset path of the AnimBP (e.g., '/Game/Animations/ABP_Character')

        Returns:
            JSON with asset_path, name, skeleton, state_machine_count, and
            state_machines array (each with name, entry_state, state_count,
            and states with their transitions)
        """
        result = await send_command("get_anim_graph", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

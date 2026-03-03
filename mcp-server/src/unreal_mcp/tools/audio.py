"""Audio tools for Unreal Engine Sound Cue creation and editing."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_audio_tools(mcp: FastMCP) -> None:
    """Register all audio MCP tools."""

    @mcp.tool()
    async def create_sound_cue(
        name: str,
        path: str,
        wave_paths: list[str],
        randomize_without_replacement: bool = True,
        pitch_min: float | None = None,
        pitch_max: float | None = None,
        volume_min: float | None = None,
        volume_max: float | None = None,
    ) -> str:
        """Create a Sound Cue asset with a Random node and optional Modulator.

        Creates a Sound Cue with a properly wired node graph:
        - Output -> [Modulator ->] Random -> WavePlayer1, WavePlayer2, ...
        - The Random node cycles through all provided SoundWaves without repetition
        - The optional Modulator adds pitch and volume variation for realism

        Args:
            name: Asset name (e.g. 'SC_Footstep_Wood')
            path: Content folder path (e.g. '/Game/Audio/Footsteps')
            wave_paths: List of SoundWave asset paths (e.g. ['/Game/Audio/Footsteps/SFX_Footstep_Wood_01', ...])
            randomize_without_replacement: If True, avoids repeating the same sound consecutively (default True)
            pitch_min: Minimum pitch multiplier for Modulator (e.g. 0.92). If any pitch/volume param is set, Modulator is added.
            pitch_max: Maximum pitch multiplier for Modulator (e.g. 1.08)
            volume_min: Minimum volume multiplier for Modulator (e.g. 0.85)
            volume_max: Maximum volume multiplier for Modulator (e.g. 1.0)

        Returns:
            JSON with asset_path, wave_count, total_nodes, has_modulator, has_random
        """
        params = {
            "name": name,
            "path": path,
            "wave_paths": wave_paths,
            "randomize_without_replacement": randomize_without_replacement,
        }
        if pitch_min is not None:
            params["pitch_min"] = pitch_min
        if pitch_max is not None:
            params["pitch_max"] = pitch_max
        if volume_min is not None:
            params["volume_min"] = volume_min
        if volume_max is not None:
            params["volume_max"] = volume_max

        result = await send_command("create_sound_cue", params)
        return str(result)

    @mcp.tool()
    async def get_sound_cue_info(
        asset_path: str,
    ) -> str:
        """Get detailed information about a Sound Cue's node graph.

        Returns all nodes in the Sound Cue with their types, properties,
        and connections. Includes type-specific details like SoundWave paths
        for WavePlayer nodes, pitch/volume ranges for Modulator nodes, etc.

        Args:
            asset_path: Sound Cue asset path (e.g. '/Game/Audio/Footsteps/SC_Footstep_Wood')

        Returns:
            JSON with name, asset_path, total_nodes, first_node_type, duration, nodes array
        """
        result = await send_command("get_sound_cue_info", {"asset_path": asset_path})
        return str(result)

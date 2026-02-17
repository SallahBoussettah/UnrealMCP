"""Asset import and Content Browser tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_asset_tools(mcp: FastMCP) -> None:
    """Register all asset management MCP tools."""

    @mcp.tool()
    async def import_asset(
        file_path: str,
        destination_path: str,
        asset_name: str = "",
        options: dict | None = None,
    ) -> str:
        """Import an external file (FBX, OBJ, PNG, JPG, WAV, etc.) into the project.

        Uses Unreal's automated asset import pipeline to detect file type and
        import with appropriate settings.

        Args:
            file_path: Absolute path to the file on disk to import
                (e.g., 'C:/Assets/chair.fbx' or 'D:/Textures/wood.png')
            destination_path: Content Browser path to import into
                (e.g., '/Game/Meshes' or '/Game/Textures')
            asset_name: Optional custom name for the imported asset.
                If empty, uses the source file name.
            options: Optional import settings dict with keys:
                - replace_existing (bool): Replace if asset already exists (default: true)
                - import_materials (bool): Import materials with meshes (default: true)
                - generate_collision (bool): Generate collision for meshes (default: true)

        Returns:
            JSON with source_file, imported_assets array (name, path, class), and count
        """
        params: dict = {
            "file_path": file_path,
            "destination_path": destination_path,
        }
        if asset_name:
            params["asset_name"] = asset_name
        if options:
            params["options"] = options

        result = await send_command("import_asset", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def search_assets(
        path: str = "",
        type: str = "",
        name_pattern: str = "",
        recursive: bool = True,
        max_results: int = 100,
    ) -> str:
        """Search the Content Browser for assets by type, path, and/or name pattern.

        Args:
            path: Content Browser path to search in (e.g., '/Game', '/Game/Meshes').
                If empty, searches all paths.
            type: Asset type filter. Supported types:
                StaticMesh, SkeletalMesh, Texture2D, Material,
                MaterialInstanceConstant, SoundWave, Blueprint, World,
                AnimSequence, ParticleSystem
            name_pattern: Case-insensitive substring to match against asset names.
                E.g., 'chair' matches 'ChairMesh', 'OldChair', etc.
            recursive: Search subdirectories of path (default: True)
            max_results: Maximum number of results to return (default: 100)

        Returns:
            JSON with assets array (name, path, package_path, class), count, and total_found
        """
        params: dict = {
            "recursive": recursive,
            "max_results": max_results,
        }
        if path:
            params["path"] = path
        if type:
            params["type"] = type
        if name_pattern:
            params["name_pattern"] = name_pattern

        result = await send_command("search_assets", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_asset_info(asset_path: str) -> str:
        """Get detailed metadata for a specific asset in the Content Browser.

        Args:
            asset_path: Full asset path (e.g., '/Game/Meshes/Chair' or
                '/Game/Meshes/Chair.Chair')

        Returns:
            JSON with name, path, package info, class, disk_size_bytes,
            is_dirty, is_loaded, referencers array, dependencies array
        """
        result = await send_command("get_asset_info", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_asset(
        asset_path: str,
        force: bool = False,
    ) -> str:
        """Delete an asset from the Content Browser with reference checking.

        By default, refuses to delete assets that are referenced by other assets.
        Use force=True to delete anyway (creates redirectors for references).

        Args:
            asset_path: Full asset path to delete (e.g., '/Game/Meshes/Chair')
            force: If True, delete even if other assets reference this one (default: False)

        Returns:
            JSON with deleted_asset name and path, or referencer info if blocked
        """
        result = await send_command("delete_asset", {
            "asset_path": asset_path,
            "force": force,
        })
        if not result.get("success"):
            error = result.get("error", "Unknown error")
            data = result.get("data")
            if data:
                return f"Error: {error}\nDetails: {data}"
            return f"Error: {error}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def rename_asset(
        asset_path: str,
        new_path: str,
    ) -> str:
        """Rename or move an asset in the Content Browser.

        Can rename in-place or move to a different folder. Automatically creates
        redirectors so existing references continue to work.

        Args:
            asset_path: Current asset path (e.g., '/Game/Meshes/OldName')
            new_path: New asset path (e.g., '/Game/Meshes/NewName' to rename,
                or '/Game/Props/Chair' to move and rename)

        Returns:
            JSON with old_path, new_path, old_name, and new_name
        """
        result = await send_command("rename_asset", {
            "asset_path": asset_path,
            "new_path": new_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

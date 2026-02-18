"""DataTable editing tools for Unreal Engine — create, read, modify, and import DataTables."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_datatable_tools(mcp: FastMCP) -> None:
    """Register all DataTable MCP tools."""

    @mcp.tool()
    async def create_data_table(
        asset_name: str,
        package_path: str,
        row_struct: str,
    ) -> str:
        """Create a new DataTable asset with a specified row struct.

        DataTables store structured data in rows, commonly used for game stats,
        item databases, dialogue lines, loot tables, and configuration data.

        Args:
            asset_name: Name for the new DataTable asset (e.g., 'DT_Items')
            package_path: Content Browser path (e.g., '/Game/Data')
            row_struct: Row struct reference — can be:
                - A C++ struct name: 'TableRowBase', 'FMyRowStruct', 'MyRowStruct'
                - A Blueprint struct path: '/Game/Structs/S_Item.S_Item'

        Returns:
            JSON with asset_path, row_struct name, columns (name + type for each field)
        """
        result = await send_command("create_data_table", {
            "asset_name": asset_name,
            "package_path": package_path,
            "row_struct": row_struct,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_data_table_row(
        asset_path: str,
        row_name: str,
        values: dict | None = None,
    ) -> str:
        """Add a new row to a DataTable with optional field values.

        Field values are passed as strings in UE text format — the same format
        shown in the DataTable editor. Examples:
        - Integer: "42"
        - Float: "3.14"
        - String/Name/Text: "Hello World"
        - Boolean: "True" or "False"
        - Vector: "(X=1.0,Y=2.0,Z=3.0)"
        - Rotator: "(Pitch=0.0,Yaw=90.0,Roll=0.0)"
        - Enum: "EnumValueName"
        - Soft object ref: "/Game/Meshes/SM_Chair.SM_Chair"

        Args:
            asset_path: DataTable asset path (e.g., '/Game/Data/DT_Items.DT_Items')
            row_name: Unique name for the new row (e.g., 'Sword', 'Row_001')
            values: Optional dict of field_name → string_value to set on the row.
                    Fields not provided will be zero-initialized.

        Returns:
            JSON with row_name, values (all fields), and total_rows count
        """
        params = {
            "asset_path": asset_path,
            "row_name": row_name,
        }
        if values is not None:
            params["values"] = values
        result = await send_command("add_data_table_row", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def modify_data_table_row(
        asset_path: str,
        row_name: str,
        values: dict,
    ) -> str:
        """Update specific fields in an existing DataTable row.

        Only the fields specified in the values dict will be changed;
        other fields remain untouched.

        Args:
            asset_path: DataTable asset path (e.g., '/Game/Data/DT_Items.DT_Items')
            row_name: Name of the row to modify
            values: Dict of field_name → string_value to update (UE text format)

        Returns:
            JSON with row_name and all current field values after modification
        """
        result = await send_command("modify_data_table_row", {
            "asset_path": asset_path,
            "row_name": row_name,
            "values": values,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def delete_data_table_row(
        asset_path: str,
        row_name: str,
    ) -> str:
        """Remove a row from a DataTable by name.

        Uses the safe editor utilities path to avoid the UE 5.6 RemoveRow crash.

        Args:
            asset_path: DataTable asset path (e.g., '/Game/Data/DT_Items.DT_Items')
            row_name: Name of the row to delete

        Returns:
            JSON with row_name and remaining_rows count
        """
        result = await send_command("delete_data_table_row", {
            "asset_path": asset_path,
            "row_name": row_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_data_table_rows(
        asset_path: str,
        row_name: str = "",
    ) -> str:
        """Get rows from a DataTable with all field values.

        Returns struct metadata (columns with names and types) plus row data.
        Can retrieve all rows or a specific row by name.

        Args:
            asset_path: DataTable asset path (e.g., '/Game/Data/DT_Items.DT_Items')
            row_name: Optional — if provided, returns only that row.
                      If empty, returns all rows.

        Returns:
            JSON with row_struct, columns [{name, type}],
            rows [{row_name, values: {field: value}}], and total_rows
        """
        result = await send_command("get_data_table_rows", {
            "asset_path": asset_path,
            "row_name": row_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def import_data_table_csv(
        asset_path: str,
        csv_data: str,
        append: bool = False,
    ) -> str:
        """Import rows into a DataTable from CSV string data.

        CSV format follows UE convention:
        - First row is the header: Name,---,Column1,Column2,...
        - "Name" column holds row names, "---" is a separator
        - Subsequent rows: RowName,,Value1,Value2,...

        Example CSV:
            Name,---,DisplayName,Damage,Weight
            Sword,,Iron Sword,25,3.5
            Shield,,Wooden Shield,0,8.0

        Args:
            asset_path: DataTable asset path (e.g., '/Game/Data/DT_Items.DT_Items')
            csv_data: Raw CSV string with header and data rows
            append: If False (default), clears the table and imports fresh.
                    If True, adds new rows without removing existing ones
                    (skips rows whose names already exist).

        Returns:
            JSON with total_rows, appended flag, errors array, and error_count
        """
        result = await send_command("import_data_table_csv", {
            "asset_path": asset_path,
            "csv_data": csv_data,
            "append": append,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

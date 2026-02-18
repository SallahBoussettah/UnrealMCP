"""UMG Widget Blueprint tools for Unreal Engine."""

from mcp.server.fastmcp import FastMCP

from ..connection import send_command


def register_widget_tools(mcp: FastMCP) -> None:
    """Register all UMG widget blueprint MCP tools."""

    @mcp.tool()
    async def create_widget_blueprint(
        name: str,
        path: str = "/Game/UI",
        root_widget_type: str = "CanvasPanel",
    ) -> str:
        """Create a new Widget Blueprint (UMG) asset.

        Args:
            name: Name for the widget blueprint (e.g., 'WBP_MainMenu', 'WBP_HUD')
            path: Content folder path (default: '/Game/UI')
            root_widget_type: Root widget type (default: 'CanvasPanel').
                Supported: CanvasPanel, VerticalBox, HorizontalBox, GridPanel,
                ScrollBox, Border, Overlay, SizeBox, WrapBox

        Returns:
            JSON with the created widget blueprint's name, asset_path, and root_widget
        """
        result = await send_command("create_widget_blueprint", {
            "name": name,
            "path": path,
            "root_widget_type": root_widget_type,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def add_widget(
        asset_path: str,
        widget_type: str,
        widget_name: str,
        parent_name: str = "",
        slot: dict | None = None,
    ) -> str:
        """Add a widget to a Widget Blueprint's widget tree.

        Args:
            asset_path: Asset path of the Widget Blueprint (e.g., '/Game/UI/WBP_HUD')
            widget_type: Type of widget to add. Supported types:
                Panels: CanvasPanel, VerticalBox, HorizontalBox, GridPanel,
                        ScrollBox, Border, Overlay, SizeBox, WrapBox
                Leaf:   Button, TextBlock, Image, ProgressBar, CheckBox,
                        Slider, EditableTextBox, Spacer
            widget_name: Name for the new widget (e.g., 'TitleText', 'PlayButton')
            parent_name: Name of the parent widget (empty = root widget)
            slot: Optional slot properties dict. For CanvasPanel parents:
                {"position": [x, y], "size": [w, h], "anchors": [minX, minY, maxX, maxY],
                 "alignment": [x, y], "padding": [left, top, right, bottom]}
                For VerticalBox/HorizontalBox parents:
                {"padding": [left, top, right, bottom],
                 "horizontal_alignment": "Fill"|"Left"|"Center"|"Right",
                 "vertical_alignment": "Fill"|"Top"|"Center"|"Bottom",
                 "size": 1.0}

        Returns:
            JSON with widget_name, widget_type, parent, and slot_class
        """
        params: dict = {
            "asset_path": asset_path,
            "widget_type": widget_type,
            "widget_name": widget_name,
            "parent_name": parent_name,
        }
        if slot is not None:
            params["slot"] = slot

        result = await send_command("add_widget", params)
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def set_widget_property(
        asset_path: str,
        widget_name: str,
        property_name: str,
        property_value: str,
    ) -> str:
        """Set a property on a widget in a Widget Blueprint.

        Args:
            asset_path: Asset path of the Widget Blueprint
            widget_name: Name of the target widget
            property_name: Property to set. Supported properties:
                Any widget: Visibility (Visible|Collapsed|Hidden|HitTestInvisible|SelfHitTestInvisible),
                            IsEnabled (true|false), ToolTipText, RenderOpacity
                TextBlock: Text, ColorAndOpacity (R,G,B[,A]), Font.Size
                Button: ColorAndOpacity (R,G,B[,A]), BackgroundColor (R,G,B[,A])
                Image: ColorAndOpacity (R,G,B[,A])
                ProgressBar: Percent (0.0-1.0), FillColorAndOpacity (R,G,B[,A])
                CheckBox: IsChecked (true|false)
                Slider: Value, MinValue, MaxValue
                EditableTextBox: Text, HintText
                Canvas slot: Position (X,Y), Size (W,H), Anchors (minX,minY,maxX,maxY), Alignment (X,Y)
                Box slot: Padding (L,T,R,B), HorizontalAlignment, VerticalAlignment
            property_value: Value as a string. Colors use comma-separated format: "1.0,0.0,0.0,1.0"

        Returns:
            JSON with widget name and property change confirmation
        """
        result = await send_command("set_widget_property", {
            "asset_path": asset_path,
            "widget_name": widget_name,
            "property_name": property_name,
            "property_value": property_value,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def get_widget_tree(asset_path: str) -> str:
        """Get the full widget hierarchy of a Widget Blueprint as JSON.

        Args:
            asset_path: Asset path of the Widget Blueprint (e.g., '/Game/UI/WBP_HUD')

        Returns:
            JSON with asset_path, name, widget_count, and recursive root tree containing
            each widget's name, class, visibility, properties, slot info, and children
        """
        result = await send_command("get_widget_tree", {
            "asset_path": asset_path,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

    @mcp.tool()
    async def remove_widget(asset_path: str, widget_name: str) -> str:
        """Remove a widget from a Widget Blueprint's widget tree.

        Cannot remove the root widget. Removing a panel widget also removes all its children.

        Args:
            asset_path: Asset path of the Widget Blueprint
            widget_name: Name of the widget to remove

        Returns:
            JSON confirming the removed widget name
        """
        result = await send_command("remove_widget", {
            "asset_path": asset_path,
            "widget_name": widget_name,
        })
        if not result.get("success"):
            return f"Error: {result.get('error', 'Unknown error')}"
        return str(result.get("data", {}))

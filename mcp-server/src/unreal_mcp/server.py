"""UnrealMCP Server - MCP Server for Unreal Engine 5.6+"""

import logging
import sys

from mcp.server.fastmcp import FastMCP

from .tools.blueprint import register_blueprint_tools
from .tools.node_graph import register_node_graph_tools
from .tools.property import register_property_tools
from .tools.actor import register_actor_tools
from .tools.viewport import register_viewport_tools
from .tools.console import register_console_tools

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    stream=sys.stderr,  # MCP uses stdout for protocol, so log to stderr
)
logger = logging.getLogger("unreal-mcp")

# Create the MCP server
mcp = FastMCP(
    "UnrealMCP",
    instructions=(
        "AI-powered control of Unreal Engine 5.6+ through the Model Context Protocol. "
        "Provides tools for Blueprint editing, node graph manipulation, property inspection, "
        "actor management, viewport screenshots, and console log reading."
    ),
)

# Register all tool modules
register_blueprint_tools(mcp)
register_node_graph_tools(mcp)
register_property_tools(mcp)
register_actor_tools(mcp)
register_viewport_tools(mcp)
register_console_tools(mcp)

logger.info("UnrealMCP server initialized with all tools registered")


def main():
    """Run the MCP server."""
    logger.info("Starting UnrealMCP server...")
    mcp.run(transport="stdio")


if __name__ == "__main__":
    main()

# UnrealMCP - AI-Powered MCP Server for Unreal Engine 5.6+

## Overview

UnrealMCP is a hybrid MCP (Model Context Protocol) server that enables AI assistants (Claude, Cursor, Windsurf, etc.) to control Unreal Engine 5.6+ through natural language. It consists of two components:

1. **Python MCP Server** - Translates MCP tool calls into JSON commands
2. **C++ UE5 Editor Plugin** - Receives commands via TCP, executes them using UE5's native C++ APIs

## Architecture

```
AI Client (Claude Code / Cursor / Windsurf)
    |
    | MCP Protocol (stdio)
    |
Python MCP Server (mcp SDK)
    |
    | TCP Socket (JSON commands) - port 55555
    |
UE5 C++ Plugin (UnrealMCP)
    |-- FBlueprintEditorUtils (blueprint CRUD)
    |-- UK2Node / UEdGraphSchema_K2 (node graph editing)
    |-- FProperty / IPropertyHandle (property inspection)
    |-- UEditorLevelLibrary (actor management)
    |-- FScreenshotRequest (viewport screenshots)
    |-- FOutputDeviceRedirector (console log reading)
```

## Target

- **Unreal Engine**: 5.6+
- **Platform**: Windows (primary), extensible to Mac/Linux
- **MCP Spec**: 2025-03-26 (Streamable HTTP compatible)

## MVP Feature Set

### 1. Blueprint CRUD
- Create new Blueprint classes (Actor, Pawn, Character, GameMode, etc.)
- List/search existing Blueprints via Asset Registry
- Read Blueprint structure (variables, functions, components, graphs)
- Delete Blueprints
- Compile Blueprints
- Duplicate Blueprints

### 2. Node Graph Editing
- Add nodes to event/function graphs (CallFunction, Event, Branch, Variable Get/Set, Cast, etc.)
- Connect pins between nodes (exec and data pins)
- Delete nodes
- Modify node properties (default values, pin values)
- List all nodes in a graph with their connections
- Create/delete custom functions
- Add/remove function parameters
- Create/delete custom events
- Add/remove variables (member and local)

### 3. Property Inspection (Details Panel)
- Read all UPROPERTYs on any UObject (actors, components, assets)
- Write/modify property values (with proper PreEditChange/PostEditChange)
- List all properties with types, categories, and metadata
- Read component hierarchy of an actor
- Read/write Class Default Object (CDO) properties
- Support for nested struct properties

### 4. Actor Management
- Spawn actors in the level (from class or Blueprint)
- Delete actors
- Set transforms (location, rotation, scale)
- Query actors by name, class, tag
- Get/set actor properties
- List all actors in the current level (world outliner)
- Duplicate actors
- Attach/detach actors

### 5. Viewport Screenshots
- Capture current viewport as PNG
- Return as base64-encoded image for AI analysis
- Configurable resolution

### 6. Console Log Reading
- Read recent console output/errors/warnings
- Filter by verbosity (Error, Warning, Log, Display)
- Subscribe to new log messages
- Clear log buffer

## Communication Protocol

### TCP JSON Protocol

Commands are sent as JSON objects over TCP, length-prefixed with a 4-byte big-endian header:

```
[4 bytes: message length][JSON payload]
```

### Command Format

```json
{
  "id": "unique-request-id",
  "command": "command_name",
  "params": {
    "key": "value"
  }
}
```

### Response Format

```json
{
  "id": "matching-request-id",
  "success": true,
  "data": { },
  "error": null
}
```

## Project Structure

```
UnrealMCP/
├── DESIGN.md                    # This file
├── mcp-server/                  # Python MCP Server
│   ├── pyproject.toml
│   ├── requirements.txt
│   └── src/unreal_mcp/
│       ├── __init__.py
│       ├── server.py            # MCP server entry point
│       ├── connection.py        # TCP connection to UE plugin
│       └── tools/
│           ├── __init__.py
│           ├── blueprint.py     # Blueprint CRUD tools
│           ├── node_graph.py    # Node graph editing tools
│           ├── property.py      # Property inspection tools
│           ├── actor.py         # Actor management tools
│           ├── viewport.py      # Viewport screenshot tools
│           └── console.py       # Console log reading tools
└── plugin/                      # UE5 C++ Editor Plugin
    └── UnrealMCP/
        ├── UnrealMCP.uplugin
        └── Source/UnrealMCP/
            ├── UnrealMCP.Build.cs
            ├── Public/
            │   ├── UnrealMCPModule.h
            │   ├── MCPTCPServer.h
            │   └── Commands/
            │       ├── MCPCommandBase.h
            │       ├── MCPBlueprintCommands.h
            │       ├── MCPActorCommands.h
            │       ├── MCPPropertyCommands.h
            │       ├── MCPNodeGraphCommands.h
            │       ├── MCPViewportCommands.h
            │       └── MCPConsoleCommands.h
            └── Private/
                ├── UnrealMCPModule.cpp
                ├── MCPTCPServer.cpp
                └── Commands/
                    ├── MCPCommandBase.cpp
                    ├── MCPBlueprintCommands.cpp
                    ├── MCPActorCommands.cpp
                    ├── MCPPropertyCommands.cpp
                    ├── MCPNodeGraphCommands.cpp
                    ├── MCPViewportCommands.cpp
                    └── MCPConsoleCommands.cpp
```

## Key UE5 APIs Used

| Feature | Primary API | Module |
|---------|------------|--------|
| Blueprint creation | `FKismetEditorUtilities::CreateBlueprint()` | UnrealEd |
| Variable management | `FBlueprintEditorUtils::AddMemberVariable()` | UnrealEd |
| Graph operations | `FBlueprintEditorUtils::AddFunctionGraph()` | UnrealEd |
| Node spawning | `UBlueprintFunctionNodeSpawner::Create()` | BlueprintGraph |
| Pin connections | `UEdGraphSchema_K2::TryCreateConnection()` | BlueprintGraph |
| Compilation | `FKismetEditorUtilities::CompileBlueprint()` | UnrealEd |
| Property access | `FProperty::ContainerPtrToValuePtr()` | CoreUObject |
| Actor spawning | `UEditorLevelLibrary::SpawnActorFromClass()` | EditorScriptingUtilities |
| Asset queries | `IAssetRegistry::GetAssets()` | AssetRegistry |
| Viewport capture | `FScreenshotRequest` | Engine |
| Console logs | `FOutputDeviceRedirector` | Core |

## Differentiation from Existing Solutions

1. **Reliable blueprint node graph editing** - Deep C++ integration using proper spawners and schema validation, not string-based hacks
2. **Atomic operations** - Multi-step changes wrapped in FScopedTransaction for undo/redo
3. **Property inspection** - Full read/write of any UPROPERTY including nested structs
4. **Console error reading** - No other MCP server does this
5. **Blueprint analysis** - Serialize entire graph structures to JSON for AI understanding
6. **Clean architecture** - Command pattern with clear separation, easy to extend

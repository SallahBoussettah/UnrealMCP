# UnrealMCP

AI-powered control of Unreal Engine 5.6+ through the [Model Context Protocol](https://modelcontextprotocol.io/) (MCP). A hybrid Python + C++ system that lets AI assistants create Blueprints, manipulate actors, edit node graphs, manage materials, inspect properties, take viewport screenshots, manage levels/maps, run Play-in-Editor sessions, create UMG widgets, build Animation Blueprints, and debug Blueprints with breakpoints — all through natural language.

**75 tools** across 14 categories, **44 Blueprint node types**, and **70 C++ command handlers**. Built for UE 5.6+.

## Architecture

```
AI Client ──stdio──> Python MCP Server ──TCP:55555──> C++ UE5 Editor Plugin
                     (75 tools)                       (70 command handlers)
                     mcp-server/                      plugin/UnrealMCP/
```

- **Python MCP Server** — Implements the MCP protocol over stdio. Translates tool calls into TCP commands.
- **C++ UE5 Plugin** — Runs inside the Unreal Editor. Listens on TCP port 55555, executes commands on the game thread using native UE5 C++ APIs.
- **Protocol** — 4-byte big-endian length prefix + JSON payload. UUID-based request/response matching. Supports payloads up to 10MB (for screenshots).

## Quick Setup

### 1. Python MCP Server

```bash
cd mcp-server
python -m venv .venv
.venv/Scripts/activate   # Windows
pip install -e .
```

### 2. UE5 Plugin

Copy `plugin/UnrealMCP/` into your UE5 project's `Plugins/` folder and compile (open the project or build from command line). Verify by checking the Output Log for:

```
UnrealMCP: Listening on port 55555
```

### 3. Claude Code Config

Add to your `.claude.json` (or use `claude mcp add`):

```json
{
  "mcpServers": {
    "UnrealMCP": {
      "command": "<path-to>/mcp-server/.venv/Scripts/python.exe",
      "args": ["-m", "unreal_mcp.server"],
      "cwd": "<path-to>/mcp-server"
    }
  }
}
```

## Tools (75 total)

### Blueprint Tools (10)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_blueprint` | Create a new Blueprint class or Interface | `name`, `parent_class` (Actor, Pawn, Character, GameModeBase, PlayerController, ActorComponent, SceneComponent), `path`, `blueprint_type` (Normal, Interface) |
| `list_blueprints` | List all Blueprint assets in a directory | `path`, `recursive` |
| `get_blueprint_info` | Get full Blueprint structure | `asset_path` → returns variables, functions, components, event graphs, parent class |
| `compile_blueprint` | Compile with detailed error diagnostics | `asset_path` → returns status, error_count, warning_count, errors[] and warnings[] with per-node details |
| `delete_blueprint` | Delete a Blueprint asset | `asset_path` |
| `add_blueprint_variable` | Add a typed member variable | `asset_path`, `variable_name`, `variable_type` (Boolean, Byte, Integer, Integer64, Float, Double, String, Text, Name, Vector, Rotator, Transform, Object:ClassName, Class:ClassName, SoftObject:ClassName, Interface:ClassName), `default_value`, `category`, `is_instance_editable` |
| `remove_blueprint_variable` | Remove a member variable | `asset_path`, `variable_name` |
| `add_blueprint_component` | Add a component to the hierarchy | `asset_path`, `component_class` (StaticMeshComponent, BoxCollisionComponent, PointLightComponent, etc.), `component_name`, `parent_component`, `location`, `rotation`, `scale` |
| `set_blueprint_component_defaults` | Set a default property on a BP component template (CDO) | `asset_path`, `component_name`, `property_name` (StaticMesh, CollisionProfileName, etc.), `property_value` (UE text format) |
| `implement_interface` | Add a Blueprint Interface to a Blueprint | `asset_path`, `interface_path` |

### Node Graph Tools (9)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `add_node` | Add a node to a Blueprint graph | `asset_path`, `node_type` (44 types — see below), `function_name`, `target_class`, `node_position`, `params` |
| `get_graph_nodes` | Get all nodes with pins and connections | `asset_path`, `graph_name` → returns node IDs, classes, positions, pin details, connection map |
| `connect_pins` | Connect an output pin to an input pin | `asset_path`, `source_node_id`, `source_pin_name`, `target_node_id`, `target_pin_name` |
| `disconnect_pins` | Break all connections from a pin | `asset_path`, `node_id`, `pin_name` |
| `delete_node` | Remove a node from a graph | `asset_path`, `node_id` |
| `set_pin_value` | Set a pin's default value | `asset_path`, `node_id`, `pin_name`, `value` (string, parsed by pin type) |
| `create_function` | Create a new function graph | `asset_path`, `function_name`, `inputs` [{name, type}], `outputs` [{name, type}] |
| `delete_function` | Delete a function from a Blueprint | `asset_path`, `function_name` |
| `arrange_graph` | Auto-layout all nodes using a layered graph algorithm | `asset_path`, `graph_name`, `horizontal_spacing`, `vertical_spacing`, `subgraph_spacing` |

**`add_node` supports 44 node types:**

| Category | node_type | Description | Required params |
|----------|-----------|-------------|-----------------|
| **Functions** | `CallFunction` | Call any UFUNCTION | `function_name`, `target_class` |
| | `CommutativeAssociativeBinaryOperator` | Expandable math (Add, Multiply...) | `function_name`, `target_class` |
| **Events** | `Event` | Built-in event | params: `event_name` |
| | `CustomEvent` | Custom event | params: `event_name` |
| | `EnhancedInputAction` | Enhanced Input event | params: `input_action_path` |
| | `Self` | Self reference | — |
| **Variables** | `VariableGet` | Get variable | params: `variable_name` |
| | `VariableSet` | Set variable | params: `variable_name` |
| **Flow Control** | `Branch` | If/else | — |
| | `Sequence` | Execution sequence | — |
| | `MultiGate` | Multiple exec outputs | — |
| | `Select` | Select by index | — |
| | `DoOnceMultiInput` | Multi-input DoOnce | — |
| | `MacroInstance` | Standard macros (ForLoop, DoOnce, WhileLoop, ForEachLoop, Gate, FlipFlop, DoN, IsValid) | params: `macro_name` |
| | `ForEachElementInEnum` | Loop over enum values | params: `enum_name` |
| **Switch** | `SwitchInteger` | Switch on int | — |
| | `SwitchString` | Switch on string | — |
| | `SwitchName` | Switch on FName | — |
| | `SwitchEnum` | Switch on enum | params: `enum_name` |
| **Casting** | `DynamicCast` | Cast To | params: `target_class` |
| | `ClassDynamicCast` | Class cast | params: `target_class` |
| **Structs** | `MakeStruct` | Make struct | params: `struct_type` |
| | `BreakStruct` | Break struct | params: `struct_type` |
| | `SetFieldsInStruct` | Set struct fields | params: `struct_type` |
| **Containers** | `MakeArray` | Make array literal | Optional: `num_inputs` |
| | `MakeMap` | Make map literal | — |
| | `MakeSet` | Make set literal | — |
| | `GetArrayItem` | Array index access | — |
| **Spawning** | `SpawnActorFromClass` | Spawn Actor node | — (set class via pin) |
| | `GenericCreateObject` | Construct Object | — (set class via pin) |
| | `AddComponentByClass` | Add Component | — (set class via pin) |
| **Delegates** | `CreateDelegate` | Create delegate binding | — |
| | `AddDelegate` | Bind to dispatcher | params: `delegate_name` |
| | `RemoveDelegate` | Unbind from dispatcher | params: `delegate_name` |
| | `CallDelegate` | Fire dispatcher | params: `delegate_name` |
| | `ClearDelegate` | Clear all bindings | params: `delegate_name` |
| **Text** | `FormatText` | Format Text with wildcards | — |
| | `EnumLiteral` | Enum value literal | params: `enum_name` |
| **Misc** | `Timeline` | Timeline | params: `timeline_name` |
| | `Knot` | Reroute node | — |
| | `LoadAsset` | Async load asset | — |
| | `EaseFunction` | Ease/interpolation | — |
| | `GetClassDefaults` | Get Class Defaults | — (set class via pin) |
| | `GetDataTableRow` | Data table lookup | — (set table via pin) |

**`add_node` examples:**
```
PrintString     → node_type="CallFunction", function_name="PrintString", target_class="KismetSystemLibrary"
Delay           → node_type="CallFunction", function_name="Delay", target_class="KismetSystemLibrary"
Branch          → node_type="Branch"
For Loop        → node_type="MacroInstance", params={"macro_name": "ForLoop"}
Do Once         → node_type="MacroInstance", params={"macro_name": "DoOnce"}
Cast To Actor   → node_type="DynamicCast", params={"target_class": "Character"}
Make Vector     → node_type="MakeStruct", params={"struct_type": "Vector"}
Break Transform → node_type="BreakStruct", params={"struct_type": "Transform"}
Spawn Actor     → node_type="SpawnActorFromClass"
Make Array      → node_type="MakeArray", params={"num_inputs": 3}
Switch on Enum  → node_type="SwitchEnum", params={"enum_name": "ECollisionChannel"}
Timeline        → node_type="Timeline", params={"timeline_name": "DoorTimeline"}
Bind Event      → node_type="AddDelegate", params={"delegate_name": "OnDamageReceived"}
Get Variable    → node_type="VariableGet", params={"variable_name": "Health"}
Custom Event    → node_type="CustomEvent", params={"event_name": "OnDamageReceived"}
Input Action    → node_type="EnhancedInputAction", params={"input_action_path": "/Game/Input/IA_Jump"}
Format Text     → node_type="FormatText"
Reroute         → node_type="Knot"
```

### Actor Tools (6)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `spawn_actor` | Spawn an actor or Blueprint instance | `actor_class` (StaticMeshActor, PointLight, CameraActor, etc.), `name`, `location`, `rotation`, `scale`, `blueprint_path` |
| `delete_actor` | Delete an actor from the level | `actor_name` |
| `set_actor_transform` | Set position, rotation, and/or scale | `actor_name`, `location`, `rotation`, `scale` (each optional — only provided values change) |
| `get_actors_in_level` | List all actors (World Outliner) | `class_filter`, `name_filter`, `tag_filter` → returns name, class, location, rotation, scale |
| `find_actors` | Search actors by name pattern | `query` (substring match) |
| `duplicate_actor` | Duplicate an actor with offset | `actor_name`, `new_name`, `location_offset` |

### Property Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_object_properties` | Read all properties (Details Panel equivalent) | `object_path` (actor name or asset path), `category_filter`, `include_inherited` → returns name, type, value, category, editability |
| `set_object_property` | Set a property with editor notifications | `object_path`, `property_name` (supports dot-notation: `RelativeLocation.X`), `property_value` |
| `get_component_hierarchy` | Get component tree | `actor_name` → returns component names, classes, relative transforms, parent hierarchy |
| `get_class_defaults` | Get Class Default Object properties | `class_name` → returns all default property values |
| `set_component_property` | Set a property on a specific component | `actor_name`, `component_name`, `property_name`, `property_value` |

### Material Tools (4)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_material` | Create a material with initial values | `name`, `path`, `base_color` [r,g,b] 0-1, `roughness` 0-1, `metallic` 0-1 |
| `assign_material` | Apply material to a mesh component | `actor_name`, `material_path`, `slot`, `component_name` |
| `modify_material` | Update material properties | `asset_path`, `base_color`, `roughness`, `metallic` (base materials); `scalar_params`, `vector_params` (material instances) |
| `get_material_info` | Get material parameters and metadata | `asset_path` → returns scalar/vector params, expression count, parent material |

### Level Tools (7)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_level_info` | Get world name, persistent level, and all streaming sub-levels with state | (none) → returns world_name, persistent_level, streaming_levels[] with visibility, loaded state, actor count, transform |
| `create_level` | Create a new blank map or from a template | `save_path`, `template_path`, `save_existing` |
| `save_level` | Save current map, Save As, or save all dirty packages | `asset_path` (Save As path), `save_all` (save everything) |
| `load_level` | Open an existing map in the editor | `map_path`, `save_existing` |
| `add_streaming_level` | Add a streaming sub-level (existing or new) | `package_name`, `streaming_class` (Dynamic, AlwaysLoaded), `location`, `rotation`, `create_new` |
| `remove_streaming_level` | Remove a streaming sub-level from the world | `package_name` |
| `set_level_visibility` | Show/hide a sub-level, optionally make it the current editing level | `package_name`, `visible`, `make_current` |

### Asset Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `import_asset` | Import external file (FBX, OBJ, PNG, JPG, WAV, etc.) into project | `file_path`, `destination_path`, `asset_name`, `options` (replace_existing, import_materials, generate_collision) |
| `search_assets` | Search Content Browser by type, path, and name pattern | `path`, `type` (StaticMesh, Texture2D, Material, Blueprint, etc.), `name_pattern`, `recursive`, `max_results` |
| `get_asset_info` | Get detailed metadata for an asset | `asset_path` → returns name, class, disk_size_bytes, is_dirty, referencers[], dependencies[] |
| `delete_asset` | Delete asset with reference checking | `asset_path`, `force` (skip reference check) |
| `rename_asset` | Rename or move asset in Content Browser | `asset_path`, `new_path` (auto-creates redirectors) |

### Viewport Tools (2)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `take_screenshot` | Capture editor window as PNG | `width`, `height`, `filename` → saves to `screenshots/` directory |
| `focus_viewport` | Move editor camera | `target` (actor name), `location` [x,y,z], `rotation` [pitch,yaw,roll], `distance` |

### Console Tools (3)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_console_logs` | Read recent log messages | `count`, `verbosity_filter` (Error, Warning, Display, Log), `category_filter` (LogBlueprint, LogTemp, LogCompile, etc.) |
| `execute_console_command` | Run a console command in the editor or PIE world | `command` (e.g., `stat fps`, `obj list`), `target` (`"editor"` or `"pie"` — default: editor) |
| `batch_execute` | Execute multiple commands in one TCP round-trip | `commands` [{command, params}, ...], `stop_on_error` |

### PIE Tools (4)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `start_pie` | Start a Play-in-Editor session | `mode` (`"viewport"`, `"new_window"`, `"simulate"`, or empty for last editor settings) |
| `stop_pie` | Stop the current PIE session | (none) |
| `get_pie_status` | Get PIE state | (none) → returns `is_running`, `is_paused`, `is_simulating`, `world_name`, `player_count` |
| `set_pie_paused` | Pause or resume the PIE session | `paused` (bool) |

### Batch Tools (3)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `batch_add_nodes` | Add multiple nodes in one call | `asset_path`, `nodes` (list of node definitions), `graph_name`, `stop_on_error` → returns `node_ids` list for wiring |
| `batch_pin_operations` | Connect pins and set values in one call | `asset_path`, `connections` [{source_node_id, source_pin, target_node_id, target_pin}], `pin_values` [{node_id, pin_name, value}] |
| `batch_spawn_actors` | Spawn multiple actors in one call | `actors` [{actor_class, name, location, rotation, scale, blueprint_path}], `stop_on_error` |

### Widget Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_widget_blueprint` | Create a UMG Widget Blueprint | `name`, `path`, `root_widget_type` (CanvasPanel, VerticalBox, etc.) |
| `add_widget` | Add a widget to the hierarchy | `asset_path`, `widget_type` (17 types: Button, TextBlock, Image, ProgressBar, etc.), `widget_name`, `parent_name`, `slot_properties` |
| `set_widget_property` | Set a widget property | `asset_path`, `widget_name`, `property_name`, `property_value` |
| `get_widget_tree` | Get full widget hierarchy with properties | `asset_path` |
| `remove_widget` | Remove a widget from the hierarchy | `asset_path`, `widget_name` |

### Animation Tools (7)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_anim_blueprint` | Create an Animation Blueprint | `name`, `skeleton_path`, `path` |
| `add_anim_state` | Add a state to AnimGraph state machine | `asset_path`, `state_name`, `animation_asset` |
| `add_anim_transition` | Add a transition between states | `asset_path`, `source_state`, `target_state`, `duration`, `blend_mode` |
| `set_anim_transition_rule` | Set transition condition | `asset_path`, `source_state`, `target_state`, `rule_type` (auto_rule, time_remaining, bool_variable) |
| `add_blend_space` | Create a BlendSpace asset | `name`, `skeleton_path`, `axis_x_name`, `axis_y_name`, `samples` |
| `add_anim_montage` | Create an AnimMontage from animation | `name`, `animation_path`, `slot_name` |
| `get_anim_graph` | Get state machine structure | `asset_path` |

### Debug Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `set_breakpoint` | Set/toggle breakpoint on a node | `asset_path`, `node_id`, `graph_name`, `enabled` |
| `get_breakpoints` | List all breakpoints in a Blueprint | `asset_path` → returns node_id, enabled, is_valid, graph_name |
| `get_watch_values` | Read watched pin values (paused PIE) | `asset_path` → returns pin values, status, current_node |
| `step_execution` | Step Blueprint debugger | `step_type` (into, over, out, resume) |
| `get_call_stack` | Get execution trace when paused | → returns current_instruction, breakpoint_hit, trace_stack[] |

## What Can You Build With This?

- **AI-assisted game prototyping** — Describe game mechanics in natural language, let the AI create Blueprints with the right nodes and connections
- **Automated Blueprint construction** — Programmatically build complex node graphs (health systems, inventory, AI behavior) and auto-layout them
- **Level design assistance** — Create/load/save maps, manage streaming sub-levels, spawn actors, set transforms, assign materials
- **Asset management** — Import meshes/textures/sounds from disk, search the Content Browser, inspect asset metadata, rename and organize assets
- **Runtime testing** — Start Play-in-Editor sessions, pause/resume gameplay, read runtime logs to verify behavior, debug at runtime
- **Debugging** — Read console logs, inspect properties, take screenshots to understand editor state
- **Batch operations** — Spawn dozens of actors, add many nodes, or wire up entire graphs in single tool calls

## Technical Details

### UE5 C++ APIs Used
- **Blueprints**: `FKismetEditorUtilities::CreateBlueprint()`, `FBlueprintEditorUtils::AddMemberVariable()`, `CompileBlueprint()`
- **Node Graph**: `UK2Node_CallFunction`, `UK2Node_Event`, `UK2Node_IfThenElse`, `UEdGraphSchema_K2::TryCreateConnection()`
- **Properties**: `FProperty::ImportText_Direct()` / `ExportTextItem_Direct()`, `PreEditChange()` / `PostEditChangeProperty()`
- **Materials**: `UMaterialFactoryNew::FactoryCreateNew()`, `UMaterial::GetEditorOnlyData()`, `UMaterialEditingLibrary`
- **Levels**: `UEditorLoadingAndSavingUtils` (NewBlankMap, LoadMap), `UEditorLevelUtils` (streaming levels, visibility), `FEditorFileUtils` (save)
- **Assets**: `UAutomatedAssetImportData` + `ImportAssetsAutomated()`, `IAssetRegistry::GetAssets()`, `UEditorAssetLibrary` (Delete, Rename)
- **PIE**: `GEditor->RequestPlaySession()`, `StartQueuedPlaySessionRequest()`, `RequestEndPlayMap()`, `SetPIEWorldsPaused()`
- **Screenshots**: Win32 `PrintWindow()` with `PW_RENDERFULLCONTENT`, `IImageWrapper` PNG encoding
- **Thread Safety**: All commands execute on game thread via `AsyncTask(ENamedThreads::GameThread)`, undo support via `FScopedTransaction`

### UE 5.6 Compatibility
This plugin integrates with UE 5.6's built-in MCP subsystem (`EpicUnrealMCPModule`). Our TCP server runs on port 55555 alongside Epic's built-in bridge on port 55557 — both operate independently without conflict.

## Requirements

- Unreal Engine 5.6+
- Python 3.10+
- MCP SDK 1.0+ (`pip install mcp`)
- Windows (primary; macOS/Linux untested)
- Visual Studio 2022 (for plugin compilation)
- An MCP-compatible AI client (Claude Code, Claude Desktop, etc.)

## Project Structure

```
UnrealMCP/
  mcp-server/                  # Python MCP server
    src/unreal_mcp/
      server.py                # Entry point, FastMCP setup
      connection.py            # TCP client (4-byte prefix protocol)
      tools/                   # Tool definitions by category
        blueprint.py           #  10 Blueprint tools
        node_graph.py          #   9 Node Graph tools
        actor.py               #   6 Actor tools
        property.py            #   5 Property tools
        material.py            #   4 Material tools
        viewport.py            #   2 Viewport tools
        console.py             #   3 Console tools (incl. batch_execute)
        level.py               #   7 Level tools
        asset.py               #   5 Asset tools
        pie.py                 #   4 PIE tools
        batch.py               #   3 Batch tools
        widget.py              #   5 Widget tools
        anim.py                #   7 Animation tools
        debug.py               #   5 Debug tools
  plugin/UnrealMCP/            # C++ UE5 editor plugin
    Source/UnrealMCP/
      Private/
        MCPTCPServer.cpp       # TCP listener, command dispatch, batch_execute
        Commands/              # 70 command handlers by category
      Public/
        Commands/              # Header files
```

## License

MIT

# UnrealMCP

The most comprehensive MCP server for Unreal Engine. **87 tools**, **45 Blueprint node types**, **16 categories** — control the entire Unreal Editor through AI.

A hybrid Python + C++ system that lets AI assistants (Claude, Cursor, Windsurf) create Blueprints, manipulate node graphs, manage actors, edit materials, build UI widgets, set up animation state machines, debug Blueprints with breakpoints, populate DataTables, configure Enhanced Input — all through natural language via the [Model Context Protocol](https://modelcontextprotocol.io/).

Built for Unreal Engine 5.6+.

## Why UnrealMCP?

| | UnrealMCP | Epic's Built-in MCP | Other Solutions |
|---|---|---|---|
| Tools | **87** | Basic editor ops | ~20-30 |
| Blueprint node types | **45** | Limited | Basic |
| Categories | **16** | Few | 3-4 |
| C++ command handlers | **82** | N/A | N/A |
| Blueprint debugging | Yes | No | No |
| Animation Blueprints | Yes | No | No |
| Widget Blueprints | Yes | No | No |
| DataTable editing | Yes | No | No |
| Enhanced Input setup | Yes | No | No |
| Production status | Production-grade | Basic | Experimental |

## Architecture

```
AI Client ──stdio──> Python MCP Server ──TCP:55555──> C++ UE5 Editor Plugin
                     (87 tools)                       (82 command handlers)
                     mcp-server/                      plugin/UnrealMCP/
```

- **Python MCP Server** — Implements the MCP protocol over stdio. Translates tool calls into TCP commands.
- **C++ UE5 Plugin** — Runs inside the Unreal Editor. Listens on TCP port 55555, executes commands on the game thread using native UE5 C++ APIs.
- **Protocol** — 4-byte big-endian length prefix + JSON payload. UUID-based request/response matching. Supports payloads up to 10MB (for screenshots).

Runs alongside Epic's built-in MCP on port 55557 — both operate independently without conflict.

## Quick Setup

### 1. Install the Python MCP Server

```bash
cd mcp-server
python -m venv .venv
.venv/Scripts/activate   # Windows
pip install -e .
```

### 2. Install the UE5 Plugin

Copy `plugin/UnrealMCP/` into your UE5 project's `Plugins/` folder and compile. Verify by checking the Output Log for:

```
UnrealMCP: Listening on port 55555
```

### 3. Configure Your AI Client

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

## What Can You Build With This?

- **AI-assisted game prototyping** — Describe game mechanics in natural language, let the AI create Blueprints with the right nodes and connections
- **Automated Blueprint construction** — Programmatically build complex node graphs (health systems, inventory, AI behavior) and auto-layout them
- **Level design** — Create/load/save maps, manage streaming sub-levels, spawn actors, set transforms, assign materials
- **Runtime testing** — Start Play-in-Editor sessions, pause/resume gameplay, read logs, debug Blueprints with breakpoints at runtime
- **UI prototyping** — Create Widget Blueprints with 17 widget types, set properties, build full UMG hierarchies
- **Animation setup** — Create Animation Blueprints, state machines, transitions, BlendSpaces, and Montages
- **Data-driven design** — Create and populate DataTables for items, stats, dialogue, loot; import from CSV
- **Input configuration** — Create InputActions and InputMappingContexts, configure WASD/gamepad bindings with modifiers and triggers
- **Batch operations** — Spawn dozens of actors, add many nodes, or wire up entire graphs in single tool calls

## Tool Reference

### Blueprint Tools (11)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_blueprint` | Create a new Blueprint class or Interface | `name`, `parent_class` (Actor, Pawn, Character, GameModeBase, PlayerController, ActorComponent, SceneComponent), `path`, `blueprint_type` (Normal, Interface) |
| `list_blueprints` | List all Blueprint assets in a directory | `path`, `recursive` |
| `get_blueprint_info` | Get full Blueprint structure | `asset_path` — returns variables, functions, components, event graphs, parent class |
| `compile_blueprint` | Compile with detailed error diagnostics | `asset_path` — returns status, error_count, warning_count, per-node error details |
| `delete_blueprint` | Delete a Blueprint asset | `asset_path` |
| `add_blueprint_variable` | Add a typed member variable | `asset_path`, `variable_name`, `variable_type` (Boolean, Integer, Float, Double, String, Vector, Rotator, Transform, Object:ClassName, etc.), `default_value`, `category`, `is_instance_editable` |
| `remove_blueprint_variable` | Remove a member variable | `asset_path`, `variable_name` |
| `add_blueprint_component` | Add a component to the hierarchy | `asset_path`, `component_class`, `component_name`, `parent_component`, `location`, `rotation`, `scale` |
| `set_blueprint_component_defaults` | Set a default property on a BP component template | `asset_path`, `component_name`, `property_name`, `property_value` |
| `remove_blueprint_component` | Remove a component from the SCS hierarchy | `asset_path`, `component_name`, `promote_children` |
| `implement_interface` | Add a Blueprint Interface to a Blueprint | `asset_path`, `interface_path` |

### Node Graph Tools (9)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `add_node` | Add a node to a Blueprint graph | `asset_path`, `node_type` (45 types — see below), `function_name`, `target_class`, `node_position`, `params` |
| `get_graph_nodes` | Get all nodes with pins and connections | `asset_path`, `graph_name` — returns node IDs, classes, positions, pin details, connection map |
| `connect_pins` | Connect an output pin to an input pin | `asset_path`, `source_node_id`, `source_pin_name`, `target_node_id`, `target_pin_name` |
| `disconnect_pins` | Break all connections from a pin | `asset_path`, `node_id`, `pin_name` |
| `delete_node` | Remove a node from a graph | `asset_path`, `node_id` |
| `set_pin_value` | Set a pin's default value | `asset_path`, `node_id`, `pin_name`, `value` |
| `create_function` | Create a new function graph | `asset_path`, `function_name`, `inputs`, `outputs` |
| `delete_function` | Delete a function from a Blueprint | `asset_path`, `function_name` |
| `arrange_graph` | Auto-layout all nodes using a layered graph algorithm | `asset_path`, `graph_name`, `horizontal_spacing`, `vertical_spacing` |

<details>
<summary><strong>45 Supported Node Types</strong> (click to expand)</summary>

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
| | `Sequence` | Execution sequence | params: `num_outputs` |
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
| **Containers** | `MakeArray` | Make array literal | `num_inputs` |
| | `MakeMap` | Make map literal | — |
| | `MakeSet` | Make set literal | — |
| | `GetArrayItem` | Array index access | — |
| **Spawning** | `SpawnActorFromClass` | Spawn Actor node | — |
| | `GenericCreateObject` | Construct Object | — |
| | `AddComponentByClass` | Add Component | — |
| | `CreateWidget` | Create Widget | params: `widget_class` |
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
| | `GetClassDefaults` | Get Class Defaults | — |
| | `GetDataTableRow` | Data table lookup | — |

**Examples:**
```
PrintString     → node_type="CallFunction", function_name="PrintString", target_class="KismetSystemLibrary"
Delay           → node_type="CallFunction", function_name="Delay", target_class="KismetSystemLibrary"
Branch          → node_type="Branch"
For Loop        → node_type="MacroInstance", params={"macro_name": "ForLoop"}
Cast To Actor   → node_type="DynamicCast", params={"target_class": "Character"}
Make Vector     → node_type="MakeStruct", params={"struct_type": "Vector"}
Spawn Actor     → node_type="SpawnActorFromClass"
Timeline        → node_type="Timeline", params={"timeline_name": "DoorTimeline"}
Input Action    → node_type="EnhancedInputAction", params={"input_action_path": "/Game/Input/IA_Jump"}
```

</details>

### Actor Tools (6)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `spawn_actor` | Spawn an actor or Blueprint instance | `actor_class`, `name`, `location`, `rotation`, `scale`, `blueprint_path` |
| `delete_actor` | Delete an actor from the level | `actor_name` |
| `set_actor_transform` | Set position, rotation, and/or scale | `actor_name`, `location`, `rotation`, `scale` |
| `get_actors_in_level` | List all actors (World Outliner) | `class_filter`, `name_filter`, `tag_filter` |
| `find_actors` | Search actors by name pattern | `query` |
| `duplicate_actor` | Duplicate an actor with offset | `actor_name`, `new_name`, `location_offset` |

### Property Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_object_properties` | Read all properties (Details Panel equivalent) | `object_path`, `category_filter`, `include_inherited` |
| `set_object_property` | Set a property with editor notifications | `object_path`, `property_name` (supports dot-notation: `RelativeLocation.X`), `property_value` |
| `get_component_hierarchy` | Get component tree | `actor_name` |
| `get_class_defaults` | Get Class Default Object properties | `class_name` |
| `set_component_property` | Set a property on a specific component | `actor_name`, `component_name`, `property_name`, `property_value` |

### Material Tools (4)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_material` | Create a material with initial values | `name`, `path`, `base_color`, `roughness`, `metallic` |
| `assign_material` | Apply material to a mesh component | `actor_name`, `material_path`, `slot`, `component_name` |
| `modify_material` | Update material properties | `asset_path`, `base_color`, `roughness`, `metallic`, `scalar_params`, `vector_params` |
| `get_material_info` | Get material parameters and metadata | `asset_path` |

### Level Tools (7)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_level_info` | Get world name, persistent level, and streaming sub-levels | — |
| `create_level` | Create a new blank map or from template | `save_path`, `template_path` |
| `save_level` | Save current map or save all dirty packages | `asset_path`, `save_all` |
| `load_level` | Open an existing map | `map_path` |
| `add_streaming_level` | Add a streaming sub-level | `package_name`, `streaming_class`, `location`, `rotation`, `create_new` |
| `remove_streaming_level` | Remove a streaming sub-level | `package_name` |
| `set_level_visibility` | Show/hide a sub-level | `package_name`, `visible`, `make_current` |

### Asset Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `import_asset` | Import FBX, OBJ, PNG, WAV, etc. | `file_path`, `destination_path`, `asset_name` |
| `search_assets` | Search Content Browser by type and name | `path`, `type`, `name_pattern`, `recursive` |
| `get_asset_info` | Get detailed asset metadata | `asset_path` |
| `delete_asset` | Delete asset with reference checking | `asset_path`, `force` |
| `rename_asset` | Rename or move asset | `asset_path`, `new_path` |

### Viewport Tools (2)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `take_screenshot` | Capture editor window as PNG | `width`, `height`, `filename` |
| `focus_viewport` | Move editor camera | `target`, `location`, `rotation`, `distance` |

### Console Tools (3)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `get_console_logs` | Read recent log messages | `count`, `verbosity_filter`, `category_filter` |
| `execute_console_command` | Run a console command | `command`, `target` (editor or pie) |
| `batch_execute` | Execute multiple commands in one TCP round-trip | `commands`, `stop_on_error` |

### PIE Tools (4)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `start_pie` | Start Play-in-Editor | `mode` (viewport, new_window, simulate) |
| `stop_pie` | Stop PIE session | — |
| `get_pie_status` | Get PIE state | — |
| `set_pie_paused` | Pause or resume | `paused` |

### Batch Tools (3)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `batch_add_nodes` | Add multiple nodes in one call | `asset_path`, `nodes`, `stop_on_error` |
| `batch_pin_operations` | Connect pins and set values in one call | `asset_path`, `connections`, `pin_values` |
| `batch_spawn_actors` | Spawn multiple actors in one call | `actors`, `stop_on_error` |

### Widget Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_widget_blueprint` | Create a UMG Widget Blueprint | `name`, `path`, `root_widget_type` |
| `add_widget` | Add a widget (17 types: Button, TextBlock, Image, ProgressBar, etc.) | `asset_path`, `widget_type`, `widget_name`, `parent_name`, `slot_properties` |
| `set_widget_property` | Set a widget property | `asset_path`, `widget_name`, `property_name`, `property_value` |
| `get_widget_tree` | Get full widget hierarchy | `asset_path` |
| `remove_widget` | Remove a widget | `asset_path`, `widget_name` |

### Animation Tools (7)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_anim_blueprint` | Create an Animation Blueprint | `name`, `skeleton_path`, `path` |
| `add_anim_state` | Add a state to AnimGraph state machine | `asset_path`, `state_name`, `animation_asset` |
| `add_anim_transition` | Add a transition between states | `asset_path`, `source_state`, `target_state`, `duration`, `blend_mode` |
| `set_anim_transition_rule` | Set transition condition | `asset_path`, `source_state`, `target_state`, `rule_type` |
| `add_blend_space` | Create a BlendSpace asset | `name`, `skeleton_path`, `axis_x_name`, `axis_y_name`, `samples` |
| `add_anim_montage` | Create an AnimMontage | `name`, `animation_path`, `slot_name` |
| `get_anim_graph` | Get state machine structure | `asset_path` |

### Debug Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `set_breakpoint` | Set/toggle breakpoint on a node | `asset_path`, `node_id`, `graph_name`, `enabled` |
| `get_breakpoints` | List all breakpoints in a Blueprint | `asset_path` |
| `get_watch_values` | Read watched pin values (paused PIE) | `asset_path` |
| `step_execution` | Step Blueprint debugger | `step_type` (into, over, out, resume) |
| `get_call_stack` | Get execution trace when paused | — |

### DataTable Tools (6)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_data_table` | Create a DataTable with a row struct | `asset_name`, `package_path`, `row_struct` |
| `add_data_table_row` | Add a row with field values | `asset_path`, `row_name`, `values` |
| `modify_data_table_row` | Update specific fields in a row | `asset_path`, `row_name`, `values` |
| `delete_data_table_row` | Remove a row | `asset_path`, `row_name` |
| `get_data_table_rows` | Get all rows with field values | `asset_path`, `row_name` |
| `import_data_table_csv` | Import rows from CSV string | `asset_path`, `csv_data`, `append` |

### Enhanced Input Tools (5)

| Tool | Description | Key Parameters |
|------|-------------|----------------|
| `create_input_action` | Create an Enhanced Input Action asset | `asset_name`, `package_path`, `value_type` (Boolean, Axis1D, Axis2D, Axis3D) |
| `create_input_mapping_context` | Create an Input Mapping Context | `asset_name`, `package_path` |
| `add_input_mapping` | Map a key to an InputAction with modifiers and triggers | `context_path`, `action_path`, `key`, `modifiers`, `triggers` |
| `remove_input_mapping` | Remove a mapping | `context_path`, `action_path`, `key` |
| `get_input_mapping_context` | Read all mappings from a context | `context_path` |

## Technical Details

### UE5 C++ APIs Used

- **Blueprints**: `FKismetEditorUtilities::CreateBlueprint()`, `FBlueprintEditorUtils::AddMemberVariable()`, `CompileBlueprint()`
- **Node Graph**: `UK2Node_CallFunction`, `UK2Node_Event`, `UK2Node_IfThenElse`, `UEdGraphSchema_K2::TryCreateConnection()`
- **Properties**: `FProperty::ImportText_Direct()` / `ExportTextItem_Direct()`, `PreEditChange()` / `PostEditChangeProperty()`
- **Materials**: `UMaterialFactoryNew::FactoryCreateNew()`, `UMaterial::GetEditorOnlyData()`, `UMaterialEditingLibrary`
- **Levels**: `UEditorLoadingAndSavingUtils`, `UEditorLevelUtils`, `FEditorFileUtils`
- **Assets**: `UAutomatedAssetImportData`, `ImportAssetsAutomated()`, `IAssetRegistry::GetAssets()`
- **PIE**: `GEditor->RequestPlaySession()`, `RequestEndPlayMap()`, `SetPIEWorldsPaused()`
- **DataTables**: `UDataTableFactory`, `FDataTableEditorUtils::AddRow()`, `CreateTableFromCSVString()`
- **Enhanced Input**: `UInputAction`, `UInputMappingContext::MapKey()`, modifiers and triggers
- **Screenshots**: Win32 `PrintWindow()`, `IImageWrapper` PNG encoding
- **Thread Safety**: All commands execute on game thread via `AsyncTask(ENamedThreads::GameThread)`, undo support via `FScopedTransaction`

## Project Structure

```
UnrealMCP/
  mcp-server/                  # Python MCP server
    src/unreal_mcp/
      server.py                # Entry point, FastMCP setup
      connection.py            # TCP client (4-byte prefix protocol)
      tools/                   # Tool definitions by category
        blueprint.py           #  11 Blueprint tools
        node_graph.py          #   9 Node Graph tools
        actor.py               #   6 Actor tools
        property.py            #   5 Property tools
        material.py            #   4 Material tools
        viewport.py            #   2 Viewport tools
        console.py             #   3 Console tools
        level.py               #   7 Level tools
        asset.py               #   5 Asset tools
        pie.py                 #   4 PIE tools
        batch.py               #   3 Batch tools
        widget.py              #   5 Widget tools
        anim.py                #   7 Animation tools
        debug.py               #   5 Debug tools
        datatable.py           #   6 DataTable tools
        input.py               #   5 Input tools
  plugin/UnrealMCP/            # C++ UE5 editor plugin
    Source/UnrealMCP/
      Private/
        MCPTCPServer.cpp       # TCP listener, command dispatch
        Commands/              # 82 command handlers by category
      Public/
        Commands/              # Header files
```

## Requirements

- Unreal Engine 5.6+
- Python 3.10+
- MCP SDK 1.0+ (`pip install mcp`)
- Windows (primary; macOS/Linux untested)
- Visual Studio 2022 (for plugin compilation)
- An MCP-compatible AI client (Claude Code, Claude Desktop, Cursor, Windsurf, etc.)

## Contributing

Contributions are welcome. See [PROGRESSION.md](PROGRESSION.md) for the roadmap and known issues.

## License

MIT

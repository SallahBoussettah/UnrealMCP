# UnrealMCP - Progress Tracker

## Done

### Core Infrastructure
- [x] Python MCP server (stdio transport, 53 tools)
- [x] C++ UE5 editor plugin (TCP port 55555, 51 command handlers + batch_execute)
- [x] 4-byte length-prefixed JSON protocol
- [x] UE 5.6 compatibility (EpicUnrealMCPModule integration, SavePackage API, module structure)
- [x] TCP reliability (null-terminate buffers, loop partial sends for large payloads)
- [x] Full editor viewport screenshots (Win32 PrintWindow + fallback window detection)
- [x] Batch operations (`batch_execute` - multiple commands in one TCP round-trip)
- [x] Material tools (create, assign, modify, get_material_info)
- [x] Level management tools (get_level_info, create_level, save_level, load_level, add/remove streaming levels, visibility)
- [x] Asset import + Content Browser tools (import_asset, search_assets, get_asset_info, delete_asset, rename_asset)
- [x] Play-in-Editor (PIE) control tools (start_pie, stop_pie, get_pie_status, set_pie_paused)
- [x] Epic BlueprintGraph utility library (node types, connectors, variables, functions)

### Bugs Fixed
- [x] **Node GUID bug** - `add_node` returned `00000000...` for all created nodes. Fixed by deferring GUID assignment to after `AddNode`/`AllocateDefaultPins` and using `FGuid::NewGuid()` fallback.
- [x] **Screenshot window detection** - `take_screenshot` failed when editor wasn't the focused window. Fixed with fallback to largest visible regular window.
- [x] **BlueprintGraph directory nesting** - Double `BlueprintGraph/BlueprintGraph/` path caused include failures. Flattened to single level.

### All 53 Tools Tested & Verified (53/53)

#### Actor (6/6)
- [x] `get_actors_in_level` - list actors with class/name/tag filtering
- [x] `find_actors` - search by name substring
- [x] `spawn_actor` - spawn with class or blueprint path, full transform
- [x] `delete_actor` - remove from level
- [x] `set_actor_transform` - position, rotation, scale (partial updates supported)
- [x] `duplicate_actor` - clone with offset

#### Blueprint (9/9)
- [x] `create_blueprint` - create BP class (Actor, Pawn, Character, etc.)
- [x] `list_blueprints` - list assets in directory, recursive
- [x] `get_blueprint_info` - variables, functions, components, graphs
- [x] `compile_blueprint` - compile and report status/errors
- [x] `delete_blueprint` - remove asset
- [x] `add_blueprint_variable` - add typed variable with category and defaults
- [x] `remove_blueprint_variable` - remove variable
- [x] `add_blueprint_component` - add component to hierarchy with transform
- [x] `set_blueprint_component_defaults` - set default property on BP component template (CDO)

#### Node Graph (8/8)
- [x] `add_node` - 43 node types across 10 categories (functions, events, variables, flow control, switch, casting, structs, containers, spawning, delegates, text, misc)
- [x] `get_graph_nodes` - all nodes with IDs, pins, connections, positions
- [x] `connect_pins` - connect by node GUID and pin name
- [x] `disconnect_pins` - break all connections on a pin
- [x] `delete_node` - remove from graph
- [x] `set_pin_value` - set default values on pins
- [x] `create_function` - new function with typed inputs/outputs
- [x] `delete_function` - remove function graph

#### Property (5/5)
- [x] `get_object_properties` - full property list (Details Panel equivalent)
- [x] `set_object_property` - set with PreEditChange/PostEditChange, dot-notation
- [x] `get_component_hierarchy` - component tree with types
- [x] `get_class_defaults` - CDO property values
- [x] `set_component_property` - set property on specific component

#### Material (4/4)
- [x] `create_material` - create with base color, roughness, metallic
- [x] `assign_material` - apply to mesh component by slot
- [x] `modify_material` - update expressions (base) or parameters (instances)
- [x] `get_material_info` - parameters, expressions, parent material

#### Viewport (2/2)
- [x] `take_screenshot` - capture editor window as PNG (base64 transport)
- [x] `focus_viewport` - camera to actor or world location

#### Console (3/3)
- [x] `get_console_logs` - filtered by verbosity/category, timestamped
- [x] `execute_console_command` - run any console command (supports `target="pie"` for PIE world)
- [x] `batch_execute` - multiple commands in single TCP round-trip

#### Level (7/7)
- [x] `get_level_info` - world name, persistent level, streaming sub-levels with state
- [x] `create_level` - blank map or from template, optional save path
- [x] `save_level` - save current, Save As, or save all dirty packages
- [x] `load_level` - open existing map with optional save of current
- [x] `add_streaming_level` - add existing or create new sub-level (Dynamic/AlwaysLoaded)
- [x] `remove_streaming_level` - remove sub-level from world
- [x] `set_level_visibility` - show/hide sub-level, optionally make current

#### Asset (5/5)
- [x] `import_asset` - import external files (FBX, OBJ, PNG, JPG, WAV, etc.) via automated pipeline
- [x] `search_assets` - search Content Browser by type, path, name pattern
- [x] `get_asset_info` - metadata, disk size, referencers, dependencies
- [x] `delete_asset` - delete with reference checking (force option)
- [x] `rename_asset` - rename/move with auto-redirectors

#### PIE (4/4)
- [x] `start_pie` - start Play-in-Editor (viewport, new_window, simulate modes)
- [x] `stop_pie` - stop current PIE session
- [x] `get_pie_status` - check running/paused/simulating state, world name, player count
- [x] `set_pie_paused` - pause or resume PIE gameplay

## Planned Upgrades

### Upgrade 1: All Blueprint Node Types (DONE)
Expanded `add_node` from 8 to 43 node types covering all major K2Node categories:
- **Flow Control** (7): Branch, Sequence, MultiGate, Select, DoOnceMultiInput, MacroInstance (ForLoop/DoOnce/WhileLoop/Gate/FlipFlop/DoN/IsValid), ForEachElementInEnum
- **Switch** (4): SwitchInteger, SwitchString, SwitchName, SwitchEnum
- **Casting** (2): DynamicCast, ClassDynamicCast
- **Structs** (3): MakeStruct, BreakStruct, SetFieldsInStruct
- **Containers** (4): MakeArray, MakeMap, MakeSet, GetArrayItem
- **Spawning** (3): SpawnActorFromClass, GenericCreateObject, AddComponentByClass
- **Delegates** (5): CreateDelegate, AddDelegate, RemoveDelegate, CallDelegate, ClearDelegate
- **Text** (2): FormatText, EnumLiteral
- **Misc** (7): Timeline, Knot, LoadAsset, EaseFunction, GetClassDefaults, GetDataTableRow, CommutativeAssociativeBinaryOperator
- **Functions/Events/Variables** (6): CallFunction, Event, CustomEvent, Self, VariableGet, VariableSet

### Upgrade 2: Compilation Error Details (DONE)
Enhanced `compile_blueprint` to return per-node error/warning details: node_id, node_title, node_class, graph name, error message, severity, and position. Enables AI auto-fix loop: create → compile → read errors → fix → recompile.

### Upgrade 3: Level Management (DONE)
Added 7 level management tools: get_level_info, create_level, save_level, load_level, add_streaming_level, remove_streaming_level, set_level_visibility. Supports blank/template map creation, Save/Save As/Save All, loading maps, and full streaming sub-level management (add, remove, visibility, make current).

### Upgrade 4: Asset Import + Content Browser (DONE)
Added 5 asset management tools: import_asset (automated pipeline for FBX/OBJ/PNG/JPG/WAV/etc.), search_assets (Content Browser search by type/path/name), get_asset_info (metadata, references, disk size), delete_asset (with reference checking), rename_asset (rename/move with redirectors).

### Upgrade 5: Play-in-Editor (PIE) Control (DONE)
Added 4 PIE control tools: start_pie (viewport/new_window/simulate modes), stop_pie, get_pie_status (running/paused/simulating state), set_pie_paused. Enhanced execute_console_command with optional `target` param to route commands to the PIE world. Closes the build→test→debug feedback loop.

### Upgrade 6: Blueprint Component Defaults (CDO) (DONE)
Added `set_blueprint_component_defaults` tool to set properties on SCS component templates (CDO). This enables setting meshes, collision profiles, materials, and other defaults on Blueprint components — things that `set_object_property` and `set_component_property` cannot reach because SCS templates are not world actors. Every spawned instance inherits the values automatically.

## Future Roadmap

- **Landscape/terrain tools** - basic terrain editing, foliage placement
- **Lighting setup** - light types, lightmap settings, post-process volumes
- **Widget/UMG Blueprint support** - create UI widgets
- **Animation Blueprint support** - state machines, blend spaces
- **Data tables / structs** - data-driven content
- **Navigation mesh / AI** - navmesh, AI controllers
- **Multi-platform testing** - macOS, Linux support

# UnrealMCP - Progress Tracker

## Done

### Core Infrastructure
- [x] Python MCP server (stdio transport, 43 tools)
- [x] C++ UE5 editor plugin (TCP port 55555, 41 command handlers + batch_execute)
- [x] 4-byte length-prefixed JSON protocol
- [x] UE 5.6 compatibility (EpicUnrealMCPModule integration, SavePackage API, module structure)
- [x] TCP reliability (null-terminate buffers, loop partial sends for large payloads)
- [x] Full editor viewport screenshots (Win32 PrintWindow + fallback window detection)
- [x] Batch operations (`batch_execute` - multiple commands in one TCP round-trip)
- [x] Material tools (create, assign, modify, get_material_info)
- [x] Level management tools (get_level_info, create_level, save_level, load_level, add/remove streaming levels, visibility)
- [x] Epic BlueprintGraph utility library (node types, connectors, variables, functions)

### Bugs Fixed
- [x] **Node GUID bug** - `add_node` returned `00000000...` for all created nodes. Fixed by deferring GUID assignment to after `AddNode`/`AllocateDefaultPins` and using `FGuid::NewGuid()` fallback.
- [x] **Screenshot window detection** - `take_screenshot` failed when editor wasn't the focused window. Fixed with fallback to largest visible regular window.
- [x] **BlueprintGraph directory nesting** - Double `BlueprintGraph/BlueprintGraph/` path caused include failures. Flattened to single level.

### All 43 Tools Tested & Verified (43/43)

#### Actor (6/6)
- [x] `get_actors_in_level` - list actors with class/name/tag filtering
- [x] `find_actors` - search by name substring
- [x] `spawn_actor` - spawn with class or blueprint path, full transform
- [x] `delete_actor` - remove from level
- [x] `set_actor_transform` - position, rotation, scale (partial updates supported)
- [x] `duplicate_actor` - clone with offset

#### Blueprint (8/8)
- [x] `create_blueprint` - create BP class (Actor, Pawn, Character, etc.)
- [x] `list_blueprints` - list assets in directory, recursive
- [x] `get_blueprint_info` - variables, functions, components, graphs
- [x] `compile_blueprint` - compile and report status/errors
- [x] `delete_blueprint` - remove asset
- [x] `add_blueprint_variable` - add typed variable with category and defaults
- [x] `remove_blueprint_variable` - remove variable
- [x] `add_blueprint_component` - add component to hierarchy with transform

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
- [x] `execute_console_command` - run any console command
- [x] `batch_execute` - multiple commands in single TCP round-trip

#### Level (7/7)
- [x] `get_level_info` - world name, persistent level, streaming sub-levels with state
- [x] `create_level` - blank map or from template, optional save path
- [x] `save_level` - save current, Save As, or save all dirty packages
- [x] `load_level` - open existing map with optional save of current
- [x] `add_streaming_level` - add existing or create new sub-level (Dynamic/AlwaysLoaded)
- [x] `remove_streaming_level` - remove sub-level from world
- [x] `set_level_visibility` - show/hide sub-level, optionally make current

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

### Upgrade 2: Compilation Error Details
Enhance `compile_blueprint` to return specific error locations — which node, which pin, what the error is. This enables an AI auto-fix loop: create → compile → read errors → fix → recompile. Currently only returns `has_errors: true/false`.

### Upgrade 3: Level Management (DONE)
Added 7 level management tools: get_level_info, create_level, save_level, load_level, add_streaming_level, remove_streaming_level, set_level_visibility. Supports blank/template map creation, Save/Save As/Save All, loading maps, and full streaming sub-level management (add, remove, visibility, make current).

### Upgrade 4: Asset Import + Content Browser
Import meshes, textures, and sounds from disk paths into the project. Search and browse existing project assets in the Content Browser. Lets AI work with real art assets, not just primitives.

### Upgrade 5: Play-in-Editor (PIE) Control
Start/stop Play-in-Editor sessions so AI can test what it built and read runtime logs to debug gameplay issues. Close the feedback loop between building and testing.

## Future Roadmap

- **Landscape/terrain tools** - basic terrain editing, foliage placement
- **Lighting setup** - light types, lightmap settings, post-process volumes
- **Widget/UMG Blueprint support** - create UI widgets
- **Animation Blueprint support** - state machines, blend spaces
- **Data tables / structs** - data-driven content
- **Navigation mesh / AI** - navmesh, AI controllers
- **Multi-platform testing** - macOS, Linux support

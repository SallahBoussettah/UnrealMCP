# UnrealMCP Progression

## Current State

**81 MCP tools** | **76 C++ command handlers** | **44 Blueprint node types** | **15 categories**

### Completed Categories

| Category | Tools | Status |
|----------|-------|--------|
| Blueprint | 10 | Done |
| Node Graph | 9 | Done |
| Actor | 6 | Done |
| Property | 5 | Done |
| Material | 4 | Done |
| Level | 7 | Done |
| Asset | 5 | Done |
| Viewport | 2 | Done |
| Console | 3 | Done |
| PIE | 4 | Done |
| Batch | 3 | Done |
| Widget | 5 | Done |
| Animation | 7 | Done |
| Debug | 5 | Done |
| DataTable | 6 | Done |

---

## Known Issues (Remaining)

### Still Open
- **Cannot create Interface Message call nodes** — e.g. "Interact (Message)" targeting BPI_Interactable
- **Cannot create Interface Event nodes** — e.g. Event Interact (from interface) must be added manually
- **OnComponentBeginOverlap / OnComponentEndOverlap** delegate bindings had to be added manually

### Fixed (v0.8)
- ~~Float variables created as Integer~~ — Fixed: uses PC_Real + PC_Float sub-category
- ~~Variable defaults not applied~~ — Fixed: C++ now reads default_value, is_instance_editable, category
- ~~Cannot create Object Reference variables~~ — Fixed: Object:ClassName, Class:ClassName, SoftObject:ClassName, Interface:ClassName
- ~~Cannot create Blueprint Interfaces~~ — Fixed: `create_blueprint(blueprint_type="Interface")`
- ~~Cannot implement Blueprint Interface~~ — Fixed: new `implement_interface` tool
- ~~Cannot add function parameters~~ — Fixed: `create_function` now accepts inputs/outputs arrays
- ~~Enhanced Input Action events~~ — Fixed in prior release (EnhancedInputAction node type)

---

## Roadmap

### Priority 1 — UMG Widget Blueprints ✓

Create and edit Widget Blueprints programmatically. Every game needs UI.

- [x] `create_widget_blueprint` — Create a Widget Blueprint asset (UserWidget parent, configurable root widget)
- [x] `add_widget` — Add 17 widget types (CanvasPanel, VerticalBox, HorizontalBox, GridPanel, ScrollBox, Border, Overlay, SizeBox, WrapBox, Button, TextBlock, Image, ProgressBar, CheckBox, Slider, EditableTextBox, Spacer) with slot properties
- [x] `set_widget_property` — Set widget properties (text, color, font size, alignment, anchors, margins, visibility, is_enabled, slot properties) with generic FProperty fallback
- [x] `get_widget_tree` — Get the full widget hierarchy with properties, slot info, and children
- [x] `remove_widget` — Remove a widget from the hierarchy
- [ ] `bind_widget_event` — Bind widget events to functions (deferred — requires event dispatcher wiring)

### Priority 2 — Animation Blueprints & State Machines ✓

- [x] `create_anim_blueprint` — Create an Animation Blueprint for a given Skeleton (with default state machine)
- [x] `add_anim_state` — Add a state to an AnimGraph state machine (with optional animation asset)
- [x] `add_anim_transition` — Add a transition between states (with crossfade duration and blend mode)
- [x] `set_anim_transition_rule` — Set the transition condition (auto_rule, time_remaining, bool_variable)
- [x] `add_blend_space` — Create a BlendSpace1D or BlendSpace2D asset (with axis config and samples)
- [x] `add_anim_montage` — Create an AnimMontage from an animation sequence (with slot and sections)
- [x] `get_anim_graph` — Get the full state machine structure (states, transitions, conditions)

### Priority 3 — Blueprint Debugging ✓

- [x] `set_breakpoint` — Set/remove a breakpoint on a Blueprint node
- [x] `get_breakpoints` — List all breakpoints in a Blueprint
- [x] `get_watch_values` — Read watched variable values during a paused PIE session
- [x] `step_execution` — Step into/over/out during Blueprint debugging
- [x] `get_call_stack` — Get the Blueprint execution call stack when paused at a breakpoint

### High Priority Bug Fixes (v0.8) ✓

- [x] Float variables: `PC_Real` + `PC_Float` sub-category (was bare `PC_Float` → Integer)
- [x] Variable defaults: C++ now reads `default_value`, `is_instance_editable`, `category`
- [x] Object/Class/SoftObject/Interface reference variable types
- [x] Function parameters: `create_function` inputs/outputs via `FUserPinInfo`
- [x] Blueprint Interface creation: `blueprint_type="Interface"` on `create_blueprint`
- [x] `implement_interface` — New tool to add interface to a Blueprint

### Priority 4 — Data Table Editing ✓

Almost every game uses DataTables for stats, items, dialogue, loot, etc.

- [x] `create_data_table` — Create a DataTable asset from a row struct
- [x] `add_data_table_row` — Add a row with field values
- [x] `modify_data_table_row` — Update specific fields in an existing row
- [x] `delete_data_table_row` — Remove a row by name (safe editor utils path, avoids UE 5.6 crash)
- [x] `get_data_table_rows` — Get all rows with their field values (or a specific row)
- [x] `import_data_table_csv` — Import rows from CSV string data (with append mode)

### Priority 5 — Sequencer / Cinematics

Create cinematic sequences and scripted events.

- [ ] `create_level_sequence` — Create a LevelSequence asset
- [ ] `add_sequence_track` — Add an actor track (transform, visibility, skeletal animation, audio, event)
- [ ] `add_sequence_keyframe` — Set a keyframe at a specific time (position, rotation, property value)
- [ ] `set_sequence_range` — Set playback range and frame rate
- [ ] `get_sequence_info` — Get tracks, keyframes, and playback settings
- [ ] `add_camera_cut_track` — Add camera cuts for cinematic shots

### Priority 6 — Enhanced Input Mapping

Complete the input pipeline (node type already exists, but no asset creation).

- [ ] `create_input_action` — Create an InputAction asset (value type: bool, float, Vector2D, Vector3D)
- [ ] `create_input_mapping_context` — Create an InputMappingContext asset
- [ ] `add_input_mapping` — Map a key/gamepad input to an InputAction with modifiers and triggers
- [ ] `get_input_mapping_context` — Get all mappings in a context

### Priority 7 — Source Control Integration

Manage version control from the AI.

- [ ] `get_source_control_status` — Get file states (checked out, added, modified, not controlled)
- [ ] `checkout_file` — Check out / mark for edit (Perforce) or stage (Git)
- [ ] `submit_files` — Submit/commit with changelist description
- [ ] `revert_file` — Revert a file to its source-controlled state

### Priority 8 — World Partition & Data Layers

For large open-world projects using UE5's World Partition.

- [ ] `get_world_partition_info` — Get partition grid settings, cell size, loading range
- [ ] `create_data_layer` — Create a Data Layer for organizing world content
- [ ] `assign_actor_data_layer` — Assign actors to data layers
- [ ] `set_data_layer_state` — Set initial runtime state (activated, loaded, unloaded)

### Priority 9 — Niagara Particle Systems

Create and modify particle effects programmatically.

- [ ] `create_niagara_system` — Create a Niagara System asset
- [ ] `add_niagara_emitter` — Add an emitter from a template
- [ ] `set_niagara_parameter` — Set module parameters (spawn rate, lifetime, color, size, velocity)
- [ ] `get_niagara_info` — Get emitters, modules, and parameter values

### Priority 10 — PCG (Procedural Content Generation)

Set up procedural generation graphs for environments.

- [ ] `create_pcg_graph` — Create a PCG Graph asset
- [ ] `add_pcg_node` — Add nodes (surface sampler, mesh spawner, density filter, bounds modifier)
- [ ] `connect_pcg_nodes` — Wire PCG graph nodes together
- [ ] `set_pcg_settings` — Configure node parameters (density, spacing, seed, exclusion zones)
- [ ] `execute_pcg_graph` — Run the PCG graph and generate content in the level

# UnrealMCP Progression

## Current State

**69 MCP tools** | **64 C++ command handlers** | **44 Blueprint node types** | **13 categories**

### Completed Categories

| Category | Tools | Status |
|----------|-------|--------|
| Blueprint | 9 | Done |
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
| Animation | 7 | Needs Test |

---

## Known Issues (HIGH PRIORITY)

Bugs and limitations discovered during real-world Blueprint building. These block common workflows.

### Blueprint Interfaces
- **Cannot create Blueprint Interfaces** — BPI assets must be created manually (Right-click > Blueprints > Blueprint Interface)
- **Cannot add function parameters to existing functions** — e.g. Caller input on Interact had to be added manually
- **Cannot implement Blueprint Interface on a class** — Adding BPI to Class Settings > Interfaces must be done manually
- **Cannot create Interface Message call nodes** — e.g. "Interact (Message)" targeting BPI_Interactable
- **Cannot create Interface Event nodes** — e.g. Event Interact (from interface) must be added manually

### Variable Type Issues
- **Cannot create Object Reference variables** — e.g. `current_interactable` (Actor ref) had to be added manually
- **Float variables created as Integer** — Variables specified as Float are created as int32. ToFloat conversion nodes auto-added as workaround
- **Variable defaults may not apply** — bCanInteract defaulted to False instead of True, OpenAngle to 0 instead of 90, open_direction to 0 instead of 1

### Missing Node Types
- **Enhanced Input Action events** — EnhancedInputAction IA_Interact had to be added manually
- **OnComponentBeginOverlap / OnComponentEndOverlap** delegate bindings had to be added manually

### Component Properties
- **GenerateOverlapEvents** — Verify setting sticks when set via `set_blueprint_component_defaults`
- **CollisionProfileName** — Verify "OverlapAllDynamic" persists after compilation

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

### Priority 2 — Animation Blueprints & State Machines (Needs Test)

AnimBPs are among the most tedious things to set up manually. Fixes applied for entry node auto-connection and bool_variable auto-creation — awaiting retest.

- [x] `create_anim_blueprint` — Create an Animation Blueprint for a given Skeleton (with default state machine)
- [x] `add_anim_state` — Add a state to an AnimGraph state machine (with optional animation asset)
- [x] `add_anim_transition` — Add a transition between states (with crossfade duration and blend mode)
- [~] `set_anim_transition_rule` — Set the transition condition (auto_rule, time_remaining, bool_variable) — **bool_variable fix untested**
- [x] `add_blend_space` — Create a BlendSpace1D or BlendSpace2D asset (with axis config and samples)
- [x] `add_anim_montage` — Create an AnimMontage from an animation sequence (with slot and sections)
- [x] `get_anim_graph` — Get the full state machine structure (states, transitions, conditions)

### Priority 3 — Blueprint Debugging

Make the AI a real debugging partner during PIE sessions.

- [ ] `set_breakpoint` — Set/remove a breakpoint on a Blueprint node
- [ ] `get_breakpoints` — List all breakpoints in a Blueprint
- [ ] `get_watch_values` — Read watched variable values during a paused PIE session
- [ ] `step_execution` — Step into/over/out during Blueprint debugging
- [ ] `get_call_stack` — Get the Blueprint execution call stack when paused at a breakpoint

### Priority 4 — Data Table Editing

Almost every game uses DataTables for stats, items, dialogue, loot, etc.

- [ ] `create_data_table` — Create a DataTable asset from a row struct
- [ ] `add_data_table_row` — Add a row with field values
- [ ] `modify_data_table_row` — Update specific fields in an existing row
- [ ] `delete_data_table_row` — Remove a row by name
- [ ] `get_data_table_rows` — Get all rows with their field values
- [ ] `import_data_table_csv` — Import rows from a CSV file

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

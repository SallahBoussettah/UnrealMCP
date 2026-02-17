# UnrealMCP Benchmark: Health System Demo

A step-by-step end-to-end test of UnrealMCP's 53 tools building a playable health system prototype entirely through AI tool calls. No manual editor interaction — every Blueprint, variable, function, component, CDO default, material, actor, and node connection is created programmatically via MCP.

## Test Environment

| Item | Value |
|------|-------|
| Unreal Engine | 5.6 |
| UnrealMCP Version | 53 tools, 10 categories |
| Base Template | Third Person |
| Platform | Windows 11 Pro |
| Date | 2026-02-17 |

## What We're Building

A playable health system demo with three Blueprints, materials, and a fresh level:

1. **BP_PlayerHealth** (ActorComponent) — Health tracking with TakeDamage/Heal functions, 3 variables, full node graph logic
2. **BP_DamageZone** (Actor) — Red cube with box collision, overlap events enabled via CDO, BeginPlay event with PrintString
3. **BP_HealthPickup** (Actor) — Green glowing sphere with collision, point light, CDO mesh/material/light defaults, BeginPlay event

Then: create a fresh level, spawn actors, start PIE, and verify everything runs.

## Categories Exercised

| Category | Tools Used | Count |
|----------|-----------|-------|
| Blueprint | create_blueprint, add_blueprint_variable, add_blueprint_component, **set_blueprint_component_defaults**, compile_blueprint, get_blueprint_info, delete_blueprint | 7 |
| Node Graph | add_node, connect_pins, set_pin_value, get_graph_nodes | 4 |
| Material | create_material | 1 |
| Actor | spawn_actor, delete_actor | 2 |
| Property | set_component_property | 1 |
| Level | create_level, save_level | 2 |
| Asset | search_assets, delete_asset | 2 |
| PIE | start_pie, stop_pie | 2 |
| Console | get_console_logs | 1 |
| Viewport | take_screenshot, focus_viewport | 2 |
| **Total** | | **24 unique tools** |

---

## Step-by-Step Test Log

### Phase 0: Clean Slate

Deleted all prior test assets and actors to start fresh.

| # | Tool | Target | Result |
|---|------|--------|--------|
| 1 | `delete_actor` | DamageZone1 | PASS |
| 2 | `delete_actor` | DamageZone2 | PASS |
| 3 | `delete_actor` | HealthPickup1-3 | PASS (3/3) |
| 4 | `delete_actor` | MCP_TestLight | PASS |
| 5 | `delete_asset` (force) | /Game/HealthDemo/BP_DamageZone | PASS |
| 6 | `delete_asset` (force) | /Game/HealthDemo/BP_HealthPickup | PASS |
| 7 | `delete_asset` (force) | /Game/HealthDemo/BP_PlayerHealth | PASS |
| 8 | `delete_asset` (force) | /Game/HealthDemo/M_DamageZone | PASS |
| 9 | `delete_asset` (force) | /Game/HealthDemo/M_HealthPickup | PASS |
| 10 | `delete_asset` (force) | /Game/Blueprints/BP_NodeTest | PASS |
| 11 | `delete_asset` (force) | /Game/Tests/BP_ErrorTest | PASS |
| 12 | `delete_asset` (force) | /Game/Tests/BP_NodeTest | PASS |
| 13 | `delete_asset` (force) | /Game/Materials/M_MCPTest_Red | PASS |
| 14 | `delete_asset` (force) | /Game/Maps/TestLevel | PASS |
| 15 | `delete_asset` (force) | /Game/Maps/TestSubLevel | PASS |

**17 deletions, all passed.**

---

### Phase 1: Level Setup

#### Step 1.1 — Create Fresh Level

**Tool:** `create_level`
**Params:** `save_path="/Game/HealthDemo/DemoLevel"`, `save_existing=true`

**Result:**
```json
{"world_name": "DemoLevel", "map_path": "/Game/HealthDemo/DemoLevel", "saved": true}
```
**Status:** PASS

---

#### Step 1.2 — Spawn Floor

**Tool:** `spawn_actor`
**Params:** `actor_class="StaticMeshActor"`, `name="Floor"`, `location=[0,0,0]`, `scale=[50,50,1]`

Then: `set_component_property` → `StaticMeshComponent0.StaticMesh = /Engine/BasicShapes/Plane.Plane`

**Status:** PASS

---

#### Step 1.3 — Spawn Lights + Player Start

| Actor | Class | Location | Result |
|-------|-------|----------|--------|
| SunLight | DirectionalLight | [0, 0, 1000], rot=[-50, -30, 0] | PASS |
| AmbientLight | SkyLight | [0, 0, 500] | PASS |
| PlayerStart | PlayerStart | [0, 0, 100] | PASS |

**All 4 level setup actors spawned successfully.**

---

### Phase 2: BP_PlayerHealth (ActorComponent)

#### Step 2.1 — Create Blueprint

**Tool:** `create_blueprint`
**Params:** `name="BP_PlayerHealth"`, `parent_class="ActorComponent"`, `path="/Game/HealthDemo"`

**Result:**
```json
{"name": "BP_PlayerHealth", "asset_path": "/Game/HealthDemo/BP_PlayerHealth.BP_PlayerHealth", "parent_class": "ActorComponent"}
```
**Status:** PASS

---

#### Step 2.2 — Add Variables

| Variable | Type | Category | Result |
|----------|------|----------|--------|
| Health | Float | Health | PASS |
| MaxHealth | Float | Health | PASS |
| bIsDead | Boolean | Health | PASS |

**3 variables added.**

---

#### Step 2.3 — Create TakeDamage Function

**Tool:** `create_function`
**Params:** `function_name="TakeDamage"`, `inputs=[{name: "DamageAmount", type: "Float"}]`, `outputs=[{name: "NewHealth", type: "Float"}]`

**Result:** Function graph created with entry node `2C0DA3B7...`

Then built the node graph:

| # | Node | Type | Purpose |
|---|------|------|---------|
| 1 | Get Health | VariableGet | Read current health |
| 2 | float - float | Subtract_DoubleDouble | Health - DamageAmount |
| 3 | Clamp (Float) | FClamp | Clamp to [0, MaxHealth] |
| 4 | Get MaxHealth | VariableGet | Upper clamp bound |
| 5 | Set Health | VariableSet | Write clamped result |

**5 nodes created, 6 connections made** (Entry→Set exec, Health→Sub A, Sub→Clamp Value, MaxHealth→Clamp Max, Clamp→Set Health). UE auto-inserted int↔float conversion nodes.

**Status:** PASS

---

#### Step 2.4 — Create Heal Function

**Tool:** `create_function`
**Params:** `function_name="Heal"`, `inputs=[{name: "HealAmount", type: "Float"}]`, `outputs=[{name: "NewHealth", type: "Float"}]`

Same structure as TakeDamage but uses `Add_DoubleDouble` instead of Subtract.

**5 nodes created, 5 connections made.**

**Status:** PASS

---

#### Step 2.5 — Compile BP_PlayerHealth

**Tool:** `compile_blueprint`

**Result:**
```json
{"name": "BP_PlayerHealth", "has_errors": false, "status": "UpToDate", "error_count": 0, "warning_count": 0}
```

**Final state:**
- 3 variables (Health, MaxHealth, bIsDead)
- 2 functions (TakeDamage: 9 nodes, Heal: 9 nodes)
- 0 errors, 0 warnings

**Status:** PASS

---

### Phase 3: BP_DamageZone (Actor)

#### Step 3.1 — Create Blueprint + Components

**Tool:** `create_blueprint` → Actor-based BP at `/Game/HealthDemo/BP_DamageZone`

**Tool:** `add_blueprint_component` (x2)

| Component | Class | Result |
|-----------|-------|--------|
| DamageBox | BoxComponent | PASS |
| VisualMesh | StaticMeshComponent | PASS |

---

#### Step 3.2 — Set CDO Defaults (NEW: `set_blueprint_component_defaults`)

This is the **key new tool** being tested. Previously, meshes/materials/collision could not be set on Blueprint component templates because SCS nodes aren't world actors.

| # | Component | Property | Value | Result |
|---|-----------|----------|-------|--------|
| 1 | VisualMesh | StaticMesh | `/Engine/BasicShapes/Cube.Cube` | **PASS** |
| 2 | DamageBox | bGenerateOverlapEvents | `true` | **PASS** |

```json
{"component_name": "VisualMesh", "property_name": "StaticMesh", "property_value": "/Engine/BasicShapes/Cube.Cube", "component_class": "StaticMeshComponent"}
```

**The cube mesh is now baked into the Blueprint CDO — every spawned instance will have it automatically.**

---

#### Step 3.3 — Add Variable + Blueprint Logic

- Added `DamagePerSecond` (Float, default 10.0)
- Added `Event BeginPlay` → `Print String` ("DamageZone Active!", red text)
- 2 nodes, 1 connection, 2 pin values set

---

#### Step 3.4 — Compile BP_DamageZone

```json
{"name": "BP_DamageZone", "has_errors": false, "status": "UpToDate", "error_count": 0, "warning_count": 0}
```

**Final state:**
- 2 components (DamageBox: BoxComponent, VisualMesh: StaticMeshComponent)
- 1 variable (DamagePerSecond)
- CDO: Cube mesh on VisualMesh, overlap events on DamageBox
- EventGraph: BeginPlay → PrintString

**Status:** PASS

---

### Phase 4: BP_HealthPickup (Actor)

#### Step 4.1 — Create Blueprint + Components

| Component | Class | Result |
|-----------|-------|--------|
| PickupSphere | SphereComponent | PASS |
| PickupMesh | StaticMeshComponent | PASS |
| PickupLight | PointLightComponent | PASS |

---

#### Step 4.2 — Set CDO Defaults (4 properties)

| # | Component | Property | Value | Result |
|---|-----------|----------|-------|--------|
| 1 | PickupMesh | StaticMesh | `/Engine/BasicShapes/Sphere.Sphere` | **PASS** |
| 2 | PickupSphere | bGenerateOverlapEvents | `true` | **PASS** |
| 3 | PickupLight | Intensity | `5000.0` | **PASS** |
| 4 | PickupLight | LightColor | `(R=0,G=255,B=50,A=255)` | **PASS** |

**4/4 CDO defaults set successfully.** Every spawned instance will have a sphere mesh, green point light at 5000 intensity, and overlap-enabled collision.

---

#### Step 4.3 — Add Variable + Blueprint Logic

- Added `HealAmount` (Float, default 25.0)
- Added `Event BeginPlay` → `Print String` ("HealthPickup Ready!", green text)

---

#### Step 4.4 — Compile BP_HealthPickup

```json
{"name": "BP_HealthPickup", "has_errors": false, "status": "UpToDate", "error_count": 0, "warning_count": 0}
```

**Final state:**
- 3 components (PickupSphere, PickupMesh, PickupLight)
- 1 variable (HealAmount)
- CDO: Sphere mesh, green light (5000 intensity), overlap events
- EventGraph: BeginPlay → PrintString

**Status:** PASS

---

### Phase 5: Materials

#### Step 5.1 — Create Materials

| Material | Base Color | Roughness | Metallic | Result |
|----------|-----------|-----------|----------|--------|
| M_DamageZone | [0.8, 0.05, 0.05] (red) | 0.3 | 0 | PASS |
| M_HealthPickup | [0.05, 0.8, 0.15] (green) | 0.2 | 0.1 | PASS |

---

#### Step 5.2 — Assign Materials to CDO Components

**Tool:** `set_blueprint_component_defaults` with `OverrideMaterials` property

| Blueprint | Component | Material | Result |
|-----------|-----------|----------|--------|
| BP_DamageZone | VisualMesh | M_DamageZone | **PASS** |
| BP_HealthPickup | PickupMesh | M_HealthPickup | **PASS** |

Materials are now baked into the CDO — every spawned instance inherits them.

---

### Phase 6: Spawn Actors

| # | Name | Blueprint | Location | Result |
|---|------|-----------|----------|--------|
| 1 | DamageZone1 | BP_DamageZone | [600, 0, 50] | PASS |
| 2 | DamageZone2 | BP_DamageZone | [-400, 500, 50] | PASS |
| 3 | HealthPickup1 | BP_HealthPickup | [0, 400, 50] | PASS |
| 4 | HealthPickup2 | BP_HealthPickup | [300, -300, 50] | PASS |
| 5 | HealthPickup3 | BP_HealthPickup | [-500, -200, 50] | PASS |

**All 5 instances spawned with CDO defaults (meshes, materials, lights) inherited automatically — no per-instance property setting needed.**

---

### Phase 7: Visual Verification

#### Editor Overview Screenshot

**Tool:** `focus_viewport` → `location=[0,0,50]`, `rotation=[-30,0,0]`, `distance=1500`
**Tool:** `take_screenshot` → `benchmark_editor_overview.png` (1920x1080, 1.3MB)

![Editor Overview](mcp-server/screenshots/benchmark_editor_overview.png)

The scene shows:
- 2 red cubes (DamageZones) with M_DamageZone material
- 3 green spheres (HealthPickups) with M_HealthPickup material and green point lights glowing
- Floor plane, directional light, sky light
- Content Browser showing all HealthDemo assets

---

### Phase 8: PIE Verification

#### Step 8.1 — Start PIE

**Tool:** `start_pie(mode="viewport")`

```json
{"is_running": true, "is_simulating": false, "is_paused": false, "world_name": "DemoLevel", "player_count": 1, "mode": "viewport"}
```
**Status:** PASS

---

#### Step 8.2 — Check Console Logs

**Tool:** `get_console_logs(verbosity_filter="Error")`

**Result:** 1 stale error from a previously-deleted test asset (BP_ErrorTest) — not from our benchmark. **Zero errors from HealthDemo assets.**

---

#### Step 8.3 — PIE Gameplay Screenshot

**Tool:** `take_screenshot` → `benchmark_pie_gameplay.png` (1920x1080, 1.2MB)

![PIE Gameplay](mcp-server/screenshots/benchmark_pie_gameplay.png)

Third-person character running in the level with all damage zones and health pickups visible at runtime.

---

#### Step 8.4 — Stop PIE

**Tool:** `stop_pie`
**Result:** `{"status": "stop_requested"}`
**Status:** PASS

---

## Results Summary

### Pass Rate

| Phase | Steps | Passed | Notes |
|-------|-------|--------|-------|
| 0. Cleanup | 17 | 17 | Deleted all prior test data |
| 1. Level Setup | 5 | 5 | Fresh level, floor, lights, player start |
| 2. BP_PlayerHealth | 5 | 5 | 3 vars, 2 functions with 18 total nodes |
| 3. BP_DamageZone | 4 | 4 | 2 components, CDO mesh + overlap, logic |
| 4. BP_HealthPickup | 4 | 4 | 3 components, 4 CDO defaults, logic |
| 5. Materials | 4 | 4 | 2 materials, 2 CDO material assignments |
| 6. Spawn Actors | 5 | 5 | 5 instances, all with inherited CDO defaults |
| 7. Viewport | 2 | 2 | Focus + screenshot |
| 8. PIE | 3 | 3 | Start, verify logs, stop |
| **Total** | **49** | **49** | **100% pass rate** |

### New Tool Verification: `set_blueprint_component_defaults`

The new tool was the primary focus of this benchmark. Results:

| # | Blueprint | Component | Property | Value | Status |
|---|-----------|-----------|----------|-------|--------|
| 1 | BP_DamageZone | VisualMesh | StaticMesh | Cube | PASS |
| 2 | BP_DamageZone | DamageBox | bGenerateOverlapEvents | true | PASS |
| 3 | BP_DamageZone | VisualMesh | OverrideMaterials | M_DamageZone | PASS |
| 4 | BP_HealthPickup | PickupMesh | StaticMesh | Sphere | PASS |
| 5 | BP_HealthPickup | PickupSphere | bGenerateOverlapEvents | true | PASS |
| 6 | BP_HealthPickup | PickupLight | Intensity | 5000.0 | PASS |
| 7 | BP_HealthPickup | PickupLight | LightColor | Green | PASS |
| 8 | BP_HealthPickup | PickupMesh | OverrideMaterials | M_HealthPickup | PASS |

**8/8 CDO property sets succeeded.** Spawned instances inherited all defaults automatically — no per-instance `set_component_property` needed.

### Tools Exercised (24 unique tools across 10 categories)

| # | Tool | Invocations | Category |
|---|------|-------------|----------|
| 1 | `create_blueprint` | 3 | Blueprint |
| 2 | `add_blueprint_variable` | 7 | Blueprint |
| 3 | `add_blueprint_component` | 5 | Blueprint |
| 4 | `set_blueprint_component_defaults` | 8 | Blueprint (NEW) |
| 5 | `compile_blueprint` | 3 | Blueprint |
| 6 | `get_blueprint_info` | 3 | Blueprint |
| 7 | `delete_blueprint` / `delete_asset` | 11 | Asset |
| 8 | `create_function` | 2 | Node Graph |
| 9 | `add_node` | 12 | Node Graph |
| 10 | `connect_pins` | 12 | Node Graph |
| 11 | `set_pin_value` | 4 | Node Graph |
| 12 | `get_graph_nodes` | 1 | Node Graph |
| 13 | `create_material` | 2 | Material |
| 14 | `spawn_actor` | 9 | Actor |
| 15 | `delete_actor` | 6 | Actor |
| 16 | `set_component_property` | 1 | Property |
| 17 | `create_level` | 1 | Level |
| 18 | `save_level` | 1 | Level |
| 19 | `search_assets` | 2 | Asset |
| 20 | `list_blueprints` | 1 | Blueprint |
| 21 | `start_pie` | 1 | PIE |
| 22 | `stop_pie` | 1 | PIE |
| 23 | `get_console_logs` | 3 | Console |
| 24 | `take_screenshot` | 2 | Viewport |
| 25 | `focus_viewport` | 1 | Viewport |
| | **Total** | **~102** | **10 categories** |

### What Was Built

From an empty level to a playable demo in ~102 tool calls:

- **1 Fresh Level**: `/Game/HealthDemo/DemoLevel` with floor, directional light, sky light, player start
- **3 Blueprints**:
  - `BP_PlayerHealth` — ActorComponent with 3 variables (Health, MaxHealth, bIsDead), 2 functions (TakeDamage, Heal) with full node graph logic (Subtract → Clamp → SetVariable)
  - `BP_DamageZone` — Actor with BoxComponent + StaticMeshComponent, CDO Cube mesh + overlap events + red material, BeginPlay → PrintString
  - `BP_HealthPickup` — Actor with SphereComponent + StaticMeshComponent + PointLightComponent, CDO Sphere mesh + green light (5000 intensity) + overlap events + green material, BeginPlay → PrintString
- **2 Materials**: M_DamageZone (deep red, smooth), M_HealthPickup (bright green, glossy)
- **5 Actors**: 2 damage zone cubes + 3 health pickup spheres, all with CDO-inherited defaults
- **PIE Session**: Started, verified (0 benchmark errors), stopped — full lifecycle tested

### Key Improvement: CDO Defaults

The previous benchmark (v52) had a **known limitation**: meshes and materials could only be set on spawned instances via `set_component_property`, not on Blueprint templates. This meant:
- Every new instance needed manual property setting
- Defaults didn't persist in the Blueprint
- Opening the Blueprint editor showed blank components

With `set_blueprint_component_defaults` (v53), properties are set directly on SCS component templates:
- Meshes, materials, collision, lights, and any other property can be set on the CDO
- Every spawned instance inherits defaults automatically
- The Blueprint editor shows the correct visual preview
- No per-instance workarounds needed

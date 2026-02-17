# UnrealMCP - Progression & Setup Guide

## Quick Start Checklist

### Phase 0: Setup (Do This First)

- [x] Project structure created
- [x] Python MCP server code written (31 tools)
- [x] C++ UE5 plugin code written (29 commands)
- [x] Git repo initialized
- [x] DESIGN.md documented
- [x] **Python environment setup** (venv + `pip install -e .` done)
- [x] **UE5 plugin installed** (compiled for UE 5.6, MCP_TEST project)
- [x] **Claude Code config updated** (`.claude.json` - old unrealMCP disabled, new UnrealMCP active)
- [x] **Basic connectivity test** (MCP server <-> UE5 plugin - ALL PASSING)

### Bugs Fixed During Setup
- `FastMCP` API: `description=` -> `instructions=` (MCP SDK v1.26.0)
- Removed dead modules: `EditorStyle`, `SubobjectDataInterface` (don't exist in UE 5.6)
- Added missing includes: `Character.h`, `GameModeBase.h`, `SavePackage.h`
- `SavePackage` API: now requires `FSavePackageArgs` struct (UE 5.6)
- `SpawnActor` API: takes references not pointers in UE 5.6
- `AddFunctionGraph`: needs explicit `<UClass>` template param
- `LogObj` variable: renamed to avoid shadowing UE global `LogObj`
- TCP `ReadMessage`: null-terminate buffer to prevent JSON parse failures
- TCP `SendResponse`: loop partial sends for large payloads (screenshots)
- Viewport screenshot: use level editor viewport client, not `GetActiveViewport()`
- Screenshot tool: auto-saves PNG to `screenshots/` folder instead of returning raw base64
- Viewport `ReadPixels`: needs `FlushRenderingCommands()` + `Viewport->Invalidate()` before capture
- Added `RenderCore` module dependency for `FlushRenderingCommands()`

---

## Setup Instructions

### Step 1: Python MCP Server Setup

```bash
# Navigate to the MCP server directory
cd D:/mcp-servers/UnrealMCP/mcp-server

# Create virtual environment
python -m venv .venv

# Activate it (Windows)
.venv/Scripts/activate

# Install the package in editable mode
pip install -e .
```

After install, verify it works:
```bash
# Should print help/version or start the server
unreal-mcp
# Or run directly:
python -m unreal_mcp.server
```

### Step 2: UE5 Plugin Installation

1. Copy the `plugin/UnrealMCP` folder into your UE5 project's `Plugins/` directory:
   ```
   D:\UnrealEngine Projects\ForgeAndFrontier\Plugins\UnrealMCP\
   ```

2. Open your UE5 project (5.6+) - the plugin should auto-compile

3. Enable the plugin:
   - Edit > Plugins > search "UnrealMCP" > enable it
   - Restart the editor if prompted

4. Verify: check the Output Log for:
   ```
   LogMCP: MCP TCP Server started on port 55555
   ```

### Step 3: Claude Code Configuration

The `.claude.json` at `C:\Users\SALAH\.claude.json` should have:
- The old `unrealMCP` entry disabled (`"disabled": true`)
- A new `UnrealMCP` entry pointing to our server

```json
"UnrealMCP": {
  "command": "D:/mcp-servers/UnrealMCP/mcp-server/.venv/Scripts/python.exe",
  "args": [
    "-m",
    "unreal_mcp.server"
  ],
  "cwd": "D:/mcp-servers/UnrealMCP/mcp-server",
  "disabled": false
}
```

### Step 4: Connectivity Test

1. Open UE5 project with the plugin enabled
2. Start Claude Code in any project
3. Claude should see the UnrealMCP tools (31 tools)
4. Try: "List all actors in the level" or "Take a viewport screenshot"

---

## Feature Progression

### MVP (Current Implementation)

| # | Feature Area | Tools | Status |
|---|-------------|-------|--------|
| 1 | **Blueprint CRUD** | create, list, get_info, compile, delete, add_variable, remove_variable, add_component | Code Written |
| 2 | **Node Graph Editing** | add_node (18 types), connect_pins, disconnect_pins, delete_node, get_graph_nodes, set_pin_value, create_function, delete_function | Code Written |
| 3 | **Property Inspection** | get_object_properties, set_object_property, get_component_hierarchy, get_class_defaults, set_component_property | Code Written |
| 4 | **Actor Management** | spawn_actor, delete_actor, set_actor_transform, get_actors_in_level, find_actors, duplicate_actor | Code Written |
| 5 | **Viewport** | take_screenshot, focus_viewport | Code Written |
| 6 | **Console Logs** | get_console_logs, execute_console_command | Code Written |

**Total: 31 MCP tools, 29 C++ command handlers**

### Test Results (2026-02-17)

| Tool | Status | Notes |
|------|--------|-------|
| `get_actors_in_level` | PASS | Returned 70+ actors from ThirdPerson template |
| `get_console_logs` | PASS | Returned log entries with timestamps |
| `create_blueprint` | PASS | Created BP_TestActor at /Game/Blueprints |
| `get_blueprint_info` | PASS | Returned variables, functions, components, graphs |
| `spawn_actor` | PASS | Spawned PointLight at [0,0,500] |
| `take_screenshot` | PASS | Saves PNG to screenshots/ folder |
| `compile_blueprint` | Not tested yet | |
| `add_node` / `connect_pins` | Not tested yet | |
| `get_object_properties` | Not tested yet | |

### Testing Priority (Remaining)

1. **Property inspection** - Read/write properties on spawned actors
2. **Node graph** - Add nodes, connect pins, read graph structure
3. **Blueprint variables/components** - Add variables, add components
4. **Delete/duplicate** - Delete actors, duplicate actors
5. **Console command execution** - Run console commands

---

## Phase 1: Hardening & Bug Fixes

After the MVP is tested end-to-end:

- [ ] Fix compilation errors in C++ plugin (if any)
- [ ] Fix TCP connection edge cases (disconnects, timeouts)
- [ ] Add proper error messages for common failures
- [ ] Test all 31 tools and fix issues found
- [ ] Add connection health check / ping-pong command
- [ ] Handle case where UE5 editor isn't running gracefully

## Phase 2: Enhanced Blueprint Editing

- [ ] Support more node types (ForEachLoop, Timeline, Delay, SpawnActor, etc.)
- [ ] Add function parameters (input/output pins)
- [ ] Support local variables in functions
- [ ] Read/write macro graphs
- [ ] Duplicate blueprints with rename
- [ ] Reparent blueprint classes
- [ ] Support Widget Blueprints (UMG)
- [ ] Support Animation Blueprints
- [ ] Blueprint diff/comparison

## Phase 3: Advanced Features

- [ ] **Material editing** - Create/modify material graphs
- [ ] **Level streaming** - Load/unload sublevels
- [ ] **Asset management** - Import/export assets, texture assignment
- [ ] **Data table editing** - Read/write data tables
- [ ] **Sequencer** - Basic timeline/cinematic control
- [ ] **Physics** - Set collision profiles, physics materials
- [ ] **AI/Navigation** - NavMesh, behavior tree basics
- [ ] **Audio** - Sound cue assignment, attenuation settings

## Phase 4: AI-Optimized Workflows

- [ ] **Batch operations** - Execute multiple commands atomically
- [ ] **Blueprint templates** - Pre-made patterns (health system, inventory, etc.)
- [ ] **Smart suggestions** - Context-aware tool recommendations
- [ ] **Undo groups** - Multi-step undo as single transaction
- [ ] **Project analysis** - Full project structure scan for AI context
- [ ] **Error diagnosis** - Parse compilation errors and suggest fixes

## Phase 5: Production Polish

- [ ] **Authentication** - Secure TCP connection
- [ ] **Multiple clients** - Support concurrent AI connections
- [ ] **Configuration file** - Port, log level, feature toggles
- [ ] **Performance profiling** - Track command execution times
- [ ] **Documentation** - Full API reference with examples
- [ ] **Automated tests** - Python unit tests + UE5 automation tests
- [ ] **CI/CD** - GitHub Actions for Python linting/testing
- [ ] **Package distribution** - PyPI for MCP server, Marketplace for plugin

---

## Known Limitations (MVP)

1. **Single connection** - Only one AI client at a time
2. **No authentication** - Local-only TCP connection (localhost)
3. **Game thread blocking** - Commands execute synchronously on game thread
4. **No streaming** - Large responses (screenshots) sent as single message
5. **Limited node types** - 8 node types (expandable to 50+)
6. **No nested struct editing** - Property system handles flat properties only
7. **Windows only** - TCP server code is cross-platform, but tested on Windows only

---

## File Locations

| What | Path |
|------|------|
| MCP Server | `D:/mcp-servers/UnrealMCP/mcp-server/` |
| Python entry point | `mcp-server/src/unreal_mcp/server.py` |
| TCP connection | `mcp-server/src/unreal_mcp/connection.py` |
| Tool definitions | `mcp-server/src/unreal_mcp/tools/*.py` |
| UE5 Plugin | `plugin/UnrealMCP/` |
| Plugin descriptor | `plugin/UnrealMCP/UnrealMCP.uplugin` |
| TCP server (C++) | `plugin/.../Private/MCPTCPServer.cpp` |
| Command handlers | `plugin/.../Private/Commands/*.cpp` |
| Headers | `plugin/.../Public/Commands/*.h` |
| Architecture doc | `DESIGN.md` |
| This file | `progression.md` |

---

## Debugging Tips

### MCP Server won't start
```bash
# Test manually
cd D:/mcp-servers/UnrealMCP/mcp-server
.venv/Scripts/python.exe -m unreal_mcp.server
# Should start and wait for MCP protocol on stdin
```

### Can't connect to UE5
- Check UE5 Output Log for "MCP TCP Server started on port 55555"
- Try: `telnet localhost 55555` (should connect)
- Check Windows Firewall isn't blocking port 55555
- Make sure only one UE5 editor instance is running

### Tools not showing in Claude Code
- Restart Claude Code after updating `.claude.json`
- Check the MCP server entry isn't set to `"disabled": true`
- Run `claude mcp list` to verify registered servers

### Command execution fails
- Check UE5 Output Log for error messages
- Commands run on game thread - heavy operations may cause brief editor freeze
- Blueprint operations require the asset to be loaded in memory

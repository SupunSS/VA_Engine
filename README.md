# VA Engine — Architecture & Developer Guide

## Overview

The VA Engine separates the **engine layer** from the **game layer**:

✅ **Developers write ONLY in `/game/` and `/assets/`**
❌ **Developers NEVER modify `/src/` (engine code)**

This is now enforced at the **build level**, not just by convention — see
[Build Targets](#build-targets) below.

This separation enables:
- **Easier updates** — the engine can be rebuilt/updated independently
- **Cleaner code** — game code is isolated from engine internals
- **No recompilation for most changes** — use Lua scripts, prefabs, and
  config files instead
- **Rapid iteration** — hot-reload and an in-engine script editor for
  instant feedback, no external IDE required
- **Reusability** — the engine can be used for multiple game projects

---

## Build Targets

The project builds three CMake targets from one `CMakeLists.txt`:

| Target | Sources | Purpose |
|---|---|---|
| `va_engine_core` | Everything in `/src/` except `src/main.cpp` | Static library containing the entire engine. Game developers never compile this themselves — they link against it. |
| `engine` | `src/main.cpp` | The engine team's own dev/test sandbox executable. |
| `game` | `game/main.cpp`, `game/GameLogic.cpp` | **The actual game executable.** This is what game developers build and run. It never touches `/src/`. |

Both `engine` and `game` link against `va_engine_core` and inherit its
include paths and compile definitions automatically.

```
cmake --build build-release --config Release
```
produces `engine.exe`, `game.exe`, and `va_engine_core.lib`.

---

## In-Engine Developer Tooling

The engine ships with a full ImGui-based editor UI (`EditorUI`), accessible
via **F1** to toggle on/off, with tools organized so different roles only
see what's relevant to their job.

### Workspaces

Menu bar → **Workspace** lets you switch which panels are visible:

| Workspace | Visible panels |
|---|---|
| **Full (Engine Dev)** | Everything |
| **Scripter** | Scene Hierarchy, Inspector, Player, Script Editor, Stats Overlay |
| **Level Designer** | Scene Hierarchy, Inspector, Asset Browser, Viewport Settings, Player, Gizmo Toolbar, Culling, Save/Load |

Individual panels can still be toggled manually at any time via the
**Window** menu, regardless of the active workspace — workspaces just set
sensible defaults, they don't lock anything.

> Workspaces currently only control panel *visibility*, not docked window
> *layout/position*. Defining custom, data-driven workspaces (e.g. via a
> config file instead of hardcoded C++) is a natural next step if more
> roles are needed later.

### Script Editor

Menu bar → **Window → Script Editor** (or included by default in the
Scripter workspace).

- Lists every `.lua` file under `game/scripts/`
- Click a file to load it into the editor
- **Save & Run** writes the file to disk, runs it immediately through the
  real `ScriptEngine`, and registers it with the engine's hot-reload
  system — so external edits (e.g. from VS Code) to the same file also
  trigger a re-run automatically, not just in-editor saves
- **Discard Changes** reloads the file from disk, dropping unsaved edits
- Won't let you switch files with unsaved changes pending, to avoid
  silently losing edits
- A Lua runtime error (bad syntax, nil reference, etc.) is logged and
  stops that script — it does **not** crash the engine. This is
  deliberate: the whole point of the in-engine editor is to let you make
  mistakes and iterate without restarting.

While typing in the Script Editor (or any ImGui text field), WASD, Space,
Ctrl, F, and Shift are suppressed from driving camera/player movement
underneath the UI, so you can type freely without accidentally moving the
camera or triggering gameplay input.

### Other panels

Scene Hierarchy, Inspector (component editing), Asset Browser (drag-drop
import, rename/delete, apply textures), Physics Test, Vehicle Test
(spawn + live-tune a vehicle), Player (Play/Stop, walk/sprint/jump
tuning), Viewport Settings, Gizmo Toolbar (move/rotate/scale via
ImGuizmo), Culling (frustum freeze + debug overlay), Save/Load.

---

## Scripting

Lua scripting runs through `ScriptEngine`, backed by [sol2](https://github.com/ThePhD/sol2).

Two API surfaces exist and are both bound into the **same** Lua state:

- **`ScriptEngine::BindEngineAPI()`** — low-level primitives: `log_info`,
  `Vec3`, `Transform`, `get_transform`, `create_entity`, `spawn_model`.
- **`RegisterLuaBindings()`** (`src/engine/LuaBindings.cpp`) — the full
  public `VAPublic::IEngine` surface, exposed as the Lua global `Engine`:
  entity spawning/destruction, transforms, physics raycasts, audio,
  input, camera, events, scripting, and lifecycle. This is what scripts
  like `game/scripts/game.lua` actually call
  (`Engine:Subscribe(...)`, `Engine:SpawnEntity(...)`, etc.).

Both are registered once, in `src/main.cpp`, after `ConnectEngineSystems()`
so the `Engine` global is fully wired before any script runs — including
scripts triggered later from the in-engine Script Editor.

```lua
-- Subscribe to engine events
Engine:Subscribe("OnGameStarted", function()
    Engine:Log("Game started!")
end)

-- Spawn from a prefab
local car = Engine:SpawnEntity("car_sport", {x=0, y=1, z=0})

-- Query/modify transforms
local pos = Engine:GetPosition(car)
Engine:SetPosition(car, {x=pos.x, y=pos.y + 1, z=pos.z})
```

See `game/scripts/game.lua` for a full example (state machine, input
handling, update loop).

---

## Prefabs

Prefabs are JSON entity templates loaded from `assets/prefabs/*.json` (or
any directory passed to `PrefabManager::LoadPrefabsFromDirectory`), spawned
via `Engine:SpawnEntity("prefab_name", position)` from Lua, or
`engine->SpawnEntity(...)` from C++.

Supported component blocks in prefab JSON:

| Component | Fields |
|---|---|
| `Transform` | `position [x,y,z]`, `rotation [x,y,z,w]`, `scale [x,y,z]` or a single number for uniform scale |
| `Model` | `meshPath`, `albedoTint [r,g,b]` |
| `Health` | `current`, `max` |
| `Ammo` | `current`, `reserve` |
| `Audio` | `clipPath`, `loop`, `autoplay`, `volume`, `minDistance`, `maxDistance` |
| `Tag` | `"Player"`, `"Vehicle"`, or `"Pedestrian"` |
| `Physics` | `type` (`"static"`/`"dynamic"`), `shape` (`"box"`/`"sphere"`), `halfExtents [x,y,z]` or `radius` |
| `Vehicle` | `maxEngineTorque`, `tireFriction`, `maxSteerAngleDegrees`, `suspensionFrequency`, `suspensionDamping` |
| `Script` | `mainScript` — runs via `ScriptEngine::RunScript` |

Example:
```json
{
  "name": "car_sport",
  "components": {
    "Transform": { "position": [0, 1, 0] },
    "Model": { "meshPath": "models/car.gltf", "albedoTint": [0.1, 0.4, 0.8] },
    "Physics": { "type": "dynamic", "shape": "box", "halfExtents": [0.9, 0.4, 1.8] },
    "Vehicle": { "maxEngineTorque": 700 },
    "Tag": "Vehicle"
  }
}
```

Unrecognized component keys are logged clearly rather than silently
ignored, so a typo in a prefab file is immediately visible.

> **Known limitation:** `Vehicle` component instantiation creates the
> physics/controller side of the vehicle, but not the four visual wheel
> mesh entities that `main.cpp`'s `SpawnTestVehicle()` creates by hand.
> See that function for the reference pattern if prefab-spawned vehicles
> need visual wheels too.
>
> **Known gap:** `Script` component execution is currently global (it
> runs the script file, same as calling `Engine:ExecuteScript` directly)
> — there is no per-entity script binding/context yet. If a script needs
> to know *which* entity it was attached to, that's a `ScriptEngine` API
> addition, not something the prefab system alone can provide.

---

## File Structure

```
VA Engine/
├── src/                          ← Engine code (READ-ONLY for developers)
│   ├── engine/
│   │   ├── public/               ← Public API headers (VAPublic namespace)
│   │   │   ├── EnginePublic.h
│   │   │   ├── Types.h
│   │   │   ├── EngineAPI.h
│   │   │   ├── EventSystem.h
│   │   │   ├── Scene.h / PhysicsWorld.h / AudioEngine.h /
│   │   │   │   ScriptingEngine.h / RenderSystem.h
│   │   ├── Adapters.h            ← Wraps real engine classes behind
│   │   │                            the VAPublic interfaces above
│   │   ├── Engine.cpp            ← IEngine implementation
│   │   ├── EventSystem.cpp
│   │   ├── LuaBindings.cpp/h     ← Binds VAPublic::IEngine into Lua
│   │   ├── Prefab.cpp/h
│   │   ├── HotReload.cpp/h
│   │   └── Config.cpp/h
│   ├── main.cpp                  ← engine.exe entry point (dev sandbox)
│   ├── editor/                   ← EditorUI, HUD
│   ├── rendering/ physics/ audio/ scene/ scripting/ core/
│
├── game/                         ← Game code (DEVELOPERS MODIFY THIS)
│   ├── main.cpp                  ← game.exe entry point
│   ├── GameLogic.h/cpp
│   ├── scripts/                  ← Lua scripts (also editable live via
│   │                                the in-engine Script Editor)
│   └── README.md
│
├── assets/                       ← Data (DEVELOPERS MODIFY THIS)
│   ├── config/
│   ├── prefabs/
│   ├── scenes/
│   ├── models/ textures/ audio/
│
└── CMakeLists.txt
```

---

## For Engine Developers (Maintaining VA Engine)

If you need to update the engine itself:

1. Modify files in `/src/engine/public/` for public API changes
2. Implement in `/src/engine/*.cpp`
3. **Never** expose internal structs (real `::Scene`, `::Transform`, etc.)
   through the public API — always go through the `VAPublic::` DTOs and
   adapters in `Adapters.h`
4. When a `.cpp` needs a `VAPublic` type unqualified for convenience,
   prefer `using namespace VAPublic;` scoped to that single `.cpp` file —
   **never** a global-scope `using` declaration in a header. Several past
   bugs in this codebase came from exactly that: a header-level
   `using VAPublic::Transform;` silently colliding with the real, different
   `::Transform` ECS component wherever both ended up included in the same
   translation unit. Qualify explicitly in headers instead.
5. Update `LuaBindings.cpp` if adding new `IEngine` functions
6. Increment version if making breaking changes

---

## Summary

✅ Engine/game separation enforced at the build level (3 CMake targets)
✅ Prefab system fully wired to the real ECS (not just a JSON parser)
✅ In-engine Script Editor with hot-reload, non-fatal error handling
✅ Role-based Workspaces so each discipline sees only relevant tools
✅ Rapid iteration via Lua + hot-reload, no restart required
✅ Data-driven via prefabs + config

**Ready to build your game — see `game/README.md` to get started.** 🎮

# Game Development Guide

This folder contains the **game layer** — the code and tools you use to
build your game on top of VA Engine.

## ✅ Do Modify (in `/game/` and `/assets/`)
- `game/main.cpp` — Game entry point
- `game/GameLogic.cpp/h` — Game logic and behavior
- `game/scripts/*.lua` — Lua scripts (editable live in the engine — see
  [Script Editor](#script-editor) below, no restart needed)
- `assets/prefabs/*.json` — Entity templates
- `assets/config/*` — Tunable settings
- Add game-specific systems, scenes, models, textures, audio

## ❌ Never Modify (in `/src/`)
- Engine source code — physics, rendering, audio, scene manager, or
  anything under `src/**`

The engine is a separate build target (`va_engine_core`) that `game.exe`
links against — you build `game.exe`, not the engine itself.

---

## Getting Started: Build & Run

```
cmake --build build-release --config Release
```

This produces `game.exe` (your actual game) alongside `engine.exe` (the
engine team's own dev sandbox — you don't need it).

Run `game.exe`. Press **F1** to toggle the developer editor UI on/off at
any time — it's the same tooling described below, available in every
build, not just a special editor mode.

---

## Pick Your Workspace

The editor UI has role-based **Workspaces** (menu bar → **Workspace**)
that show only the tools relevant to what you're doing, instead of every
panel at once:

- **Scripter** — Script Editor, Scene Hierarchy, Inspector, Player
  controls, performance stats. Everything else hidden.
- **Level Designer** — Scene Hierarchy, Inspector, Asset Browser,
  Viewport Settings, Gizmo Toolbar, Culling, Save/Load. No script editor.
- **Full (Engine Dev)** — everything, if you want it all.

Switching workspaces just changes what's *visible* — you can always
manually re-enable any individual panel via the **Window** menu if you
need something outside your current workspace momentarily.

---

## Script Editor — write & test Lua without leaving the engine

Menu bar → **Window → Script Editor** (on by default in the Scripter
workspace).

1. Pick a `.lua` file from the list on the left (reads from
   `game/scripts/`)
2. Edit it in the text area on the right
3. Click **Save & Run** — this writes the file, runs it immediately, and
   sets it up so future external edits to that same file (e.g. saving it
   from VS Code while the engine is running) also auto-reload
4. If your script has an error, it's logged to the console and that
   script stops — **the engine keeps running**. Fix the typo and hit
   Save & Run again.

While typing in the editor (or any text field), WASD/Space/Ctrl/F/Shift
won't move the camera or trigger gameplay underneath you.

---

## Basic Workflow

### 1. Spawn entities from prefabs
```cpp
EntityId player = engine->SpawnEntity("player_character", Vec3(0, 1, 0));
EntityId car = engine->SpawnEntity("car_sport", Vec3(10, 1, 0));
```
or from Lua:
```lua
local car = Engine:SpawnEntity("car_sport", {x=10, y=1, z=0})
```

### 2. Handle input
```cpp
InputState input = engine->GetInput();
if (engine->IsKeyHeld(KeyCode::W)) {
    // Move forward
}
```

### 3. Use events
```lua
Engine:Subscribe("OnCollisionEnter", function()
    -- handle collision
end)
```

### 4. Build entity templates as prefabs, not code

Instead of hand-writing spawn code, define entities as data in
`assets/prefabs/*.json`. Supported blocks:

| Component | Fields |
|---|---|
| `Transform` | `position`, `rotation` (quaternion `[x,y,z,w]`), `scale` (array or single uniform number) |
| `Model` | `meshPath`, `albedoTint` |
| `Health` | `current`, `max` |
| `Ammo` | `current`, `reserve` |
| `Audio` | `clipPath`, `loop`, `autoplay`, `volume`, `minDistance`, `maxDistance` |
| `Tag` | `"Player"` / `"Vehicle"` / `"Pedestrian"` |
| `Physics` | `type` (`static`/`dynamic`), `shape` (`box`/`sphere`), `halfExtents` or `radius` |
| `Vehicle` | `maxEngineTorque`, `tireFriction`, `maxSteerAngleDegrees`, `suspensionFrequency`, `suspensionDamping` |
| `Script` | `mainScript` |

```json
{
  "name": "npc_pedestrian",
  "components": {
    "Transform": { "position": [0, 0, 0] },
    "Model": { "meshPath": "models/npc.fbx" },
    "Tag": "Pedestrian",
    "Health": { "max": 50 }
  }
}
```
Any component key that's misspelled or unrecognized is logged clearly at
load time — check the console if a prefab isn't behaving as expected.

---

## File Structure

```
/game/                      ← Your game layer
    main.cpp                ← Entry point
    GameLogic.cpp/.h         ← Main game controller
    scripts/                 ← Lua scripts — edit live via Script Editor

/assets/                    ← Data and configs
    scenes/
        level1.json
    prefabs/
        car_sport.json
        npc_pedestrian.json
    config/
        game.yaml            ← Game settings

/src/                       ← Engine code (READ-ONLY, don't touch)
    engine/public/           ← Public API — this is ALL you should
                                 ever need to #include from /game/
```

---

## Using the Engine API (C++ side)

```cpp
#include <engine/public/EnginePublic.h>

IEngine* engine = GetEngine();

// Rendering
engine->GetRender().SetCameraPosition(Vec3(0, 5, -10));

// Physics
RaycastHit hit = engine->Raycast(origin, direction, 100.0f);

// Audio
engine->PlaySound("audio/explosion.wav", &position);

// Scripting
engine->ExecuteScript("scripts/game.lua");

// Events
engine->GetEvents().Subscribe(Events::OnEntitySpawned, callback);
```

### Recommended pattern: system-based architecture

```cpp
class PlayerController {
public:
    PlayerController(IEngine* engine) : m_engine(engine) {}

    void Update(float deltaTime) {
        InputState input = m_engine->GetInput();
        // Handle input and update player
    }

private:
    IEngine* m_engine;
};

// In GameLogic::Update
void GameLogic::Update(float deltaTime, const InputState& input) {
    m_playerController.Update(deltaTime);
    m_aiSystem.Update(deltaTime);
    m_questSystem.Update(deltaTime);
}
```

---

## Known Gaps (as of this build)

- **Vehicle prefabs** don't yet auto-create the four visual wheel
  entities — only the physics/controller side. See `SpawnTestVehicle()`
  in `src/main.cpp` for the reference pattern if you need visual wheels
  on a prefab-spawned vehicle.
- **Script components on prefabs** run the script file globally — there's
  no per-entity script context yet (a script can't currently ask "which
  entity am I attached to?").
- **Workspaces** currently control panel visibility only, not saved
  window layout/position.

---

## Next Steps

1. Try the **Scripter workspace** and edit `game/scripts/game.lua` live
2. Define your own prefabs in `/assets/prefabs/`
3. Add game systems in `/game/` (e.g. `PlayerController`, `AISystem`)
4. Use the **Level Designer workspace** to place and tune the world
5. Config-drive everything you can via `/assets/config/`
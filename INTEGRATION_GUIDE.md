# Connecting the Public API to Existing Engine

The public API (`namespace VAPublic`) is ready to integrate with the real
engine systems in `/src/`. This is already done for you in `src/main.cpp`
and `src/engine/Engine.cpp` — this doc explains how it works and how to
extend it, e.g. if you're wiring up a second entry point (like `game/main.cpp`)
or adding a new system to connect.

## How to Connect

```cpp
#include <engine/public/EnginePublic.h>

int main() {
    // ... create real engine systems ...
    Scene scene;
    PhysicsWorld physicsWorld;
    AudioEngine& audioEngine = AudioEngine::Get();
    ScriptEngine scriptEngine;
    Camera camera;

    // Initialize the public API engine
    VAPublic::IEngine* engine = VAPublic::InitializeEngine("config/engine.yaml");

    // Connect real systems to the public API
    VAPublic::ConnectEngineSystems(engine, &scene, &physicsWorld, &audioEngine, &scriptEngine, &camera);

    // Bind the same API into Lua so scripts can call it too — see
    // "Scripting" in the root README.md. Must happen after
    // ConnectEngineSystems() so `engine` is fully wired first.
    RegisterLuaBindings(scriptEngine.GetLuaState(), engine);

    // Now you can use the public API:
    VAPublic::EntityId car = engine->SpawnEntity("car_sport", VAPublic::Vec3(0, 1, 0));
    engine->PlaySound("engine_start.wav");

    // ... main loop ...

    VAPublic::ShutdownEngine();
    return 0;
}
```

> **Note the include path**: `<engine/public/EnginePublic.h>`, not
> `<engine/EnginePublic.h>` — the real file lives under `src/engine/public/`.
> A `game/` file using the wrong path will fail to compile with a "cannot
> open include file" error.
>
> **Note the namespace**: everything from the public API lives in
> `namespace VAPublic`. In a **header**, always qualify explicitly
> (`VAPublic::IEngine`, `VAPublic::Vec3`, etc.) — never add a global-scope
> `using VAPublic::X;` to a header, since several `VAPublic` type names
> collide with real, different engine classes of the same name (most
> notably `VAPublic::Transform` vs. the real ECS `::Transform`). In a
> `.cpp` file it's safe to write `using namespace VAPublic;` once at the
> top, since that scope never leaks into other files.

## What Gets Connected

| System | Real Class | How to Access |
|--------|-----------|---------------|
| Scene | `::Scene` | `engine->GetScene()` or `engine->GetRegistry()` |
| Physics | `::PhysicsWorld` | `engine->GetPhysics()` |
| Audio | `::AudioEngine` | `engine->GetAudio()` |
| Scripting | `::ScriptEngine` | `engine->GetScripting()` |
| Rendering | `::Camera` | `engine->GetCameraPosition()` etc. |

Internally, each of these is wrapped by an adapter class in
`src/engine/Adapters.h` (`SceneAdapter`, `PhysicsWorldAdapter`,
`AudioEngineAdapter`, `ScriptingEngineAdapter`, `RenderSystemAdapter`) —
each one inherits from the matching `VAPublic::I...` interface and holds a
pointer to the real object. Game code never sees the adapters directly,
only the `VAPublic::IEngine` interface.

## Example: Call From Game Code

```cpp
// game/GameLogic.cpp
#include <engine/public/EnginePublic.h>

using namespace VAPublic;

void GameLogic::Update(float deltaTime) {
    IEngine* engine = GetEngine();

    if (engine->IsKeyHeld(KeyCode::W)) {
        engine->PlaySound("footstep.wav");
    }

    EntityId npc = engine->SpawnEntity("npc_pedestrian", position);
    engine->GetEvents().Subscribe("OnEntityDestroyed", [](const void*) {
        engine->Log("Entity destroyed");
    });
}
```

## How SpawnEntity Actually Works

`engine->SpawnEntity(prefabName, position)`:
1. Looks up `prefabName` in `PrefabManager` — fails with a logged error
   and returns `0` if it isn't loaded.
2. Creates a real entity in the connected `Scene`.
3. Applies every component block defined in that prefab's JSON (see the
   **Prefabs** section of the root `README.md` for the full list of
   supported component types).
4. If the prefab has a `Physics` or `Vehicle` block, those only take
   effect if a `PhysicsWorld` was connected via `ConnectEngineSystems`.
   Same for `Script` blocks and the connected `ScriptEngine`. If the
   relevant system wasn't connected, that component block is skipped
   with a logged message — it doesn't crash, and every other component in
   the same prefab still applies normally.

This means `SpawnEntity` on an *unconnected* engine (e.g. `Phase 1` below,
where only `Scene` is passed in) will still create the entity and apply
`Transform`/`Model`/`Health`/`Ammo`/`Audio`/`Tag` components from the
prefab — it just silently skips any `Physics`/`Vehicle`/`Script` blocks
until those systems are connected too.

## Current Dummy Systems

If a system isn't connected, `IEngine` falls back to dummy implementations
that don't crash, return sensible defaults, and log where relevant. This
lets you integrate systems gradually:

```cpp
// Phase 1: Connect one system at a time
VAPublic::ConnectEngineSystems(engine, &scene, nullptr, nullptr, nullptr, nullptr);

// Phase 2: Add more as they're tested
VAPublic::ConnectEngineSystems(engine, &scene, &physicsWorld, nullptr, nullptr, nullptr);

// Phase 3: Complete integration
VAPublic::ConnectEngineSystems(engine, &scene, &physicsWorld, &audioEngine, &scriptEngine, &camera);
```

## Next Steps

1. **Locate your `main()`** — find where all real engine systems are created (already done in `src/main.cpp`)
2. **Add the include** — `#include <engine/public/EnginePublic.h>`
3. **Call `ConnectEngineSystems()`** — pass your real system pointers
4. **Call `RegisterLuaBindings()`** — if you want Lua scripts to use the same API (see root `README.md` → Scripting)
5. **Test** — call `engine->SpawnEntity()` or other API methods and confirm behavior
6. **Iterate** — gradually replace hardcoded logic with public API calls

## Troubleshooting

### "Entity not spawning"
- Check the log — `SpawnEntity` always logs a clear reason on failure
  (prefab not found, or scene not connected)
- Is the prefab loaded? `GetPrefabManager().HasPrefab("name")`
- Is the scene connected? `ConnectEngineSystems` must have been called
  with a non-null `Scene*`
- Is the entity valid afterward? `engine->IsEntityValid(id)` should
  return `true`

### "Prefab spawns but has no physics body / isn't a working vehicle"
- `Physics` and `Vehicle` prefab component blocks require a `PhysicsWorld*`
  to have been passed to `ConnectEngineSystems` — check the log for
  `"Physics component present but no PhysicsWorld was provided — skipping"`

### "Script component on a prefab doesn't run"
- Requires a `ScriptEngine*` passed to `ConnectEngineSystems` — check the
  log for a similar "no ScriptEngine was provided" message
- Note: prefab `Script` blocks currently run the script file globally
  (same as `Engine:ExecuteScript`) — there's no per-entity script context
  yet

### "Can't access scene registry"
- Ensure a `Scene*` was passed to `ConnectEngineSystems()`
- `GetRegistry()` throws if the scene was never connected

### "Dummy systems being used"
- Check whether the relevant pointer was actually passed to
  `ConnectEngineSystems()` (not `nullptr`)
- Use `engine->IsDeveloperMode()` to gate extra debug logging around this

### "cannot open include file: engine/EnginePublic.h"
- Wrong path — it's `<engine/public/EnginePublic.h>`

### "identifier 'IEngine'/'Vec3'/'Transform' is undeclared"
- Missing `VAPublic::` qualification, or (in a `.cpp`) missing
  `using namespace VAPublic;` at the top of the file

## Architecture Notes

The public API uses:
- **Adapter classes** (`Adapters.h`) wrapping real engine objects behind
  the `VAPublic::I...` interfaces — never raw `void*` casts in game-facing
  code
- **Dummy implementations** for any system not connected
- **Data-driven entity creation** via `PrefabManager`, not hardcoded spawn
  logic per entity type
- A hard split: `/src/` (outside `engine/public/`) has zero knowledge of
  the `game/` layer, and `game/` never includes anything from `/src/`
  except `engine/public/EnginePublic.h`
# Connecting the Public API to Existing Engine

The public API is now ready to be integrated with your existing engine systems in `/src/`.

## How to Connect

In your `main()` function (or wherever you initialize engine systems), add these calls:

```cpp
#include <engine/EnginePublic.h>

int main() {
    // ... create existing systems ...
    
    Scene scene;
    PhysicsWorld physicsWorld;
    AudioEngine& audioEngine = AudioEngine::Get();
    ScriptEngine scriptEngine;
    Camera camera;
    
    // Initialize the public API engine
    IEngine* engine = InitializeEngine("config/engine.yaml");
    
    // Connect real systems to the public API
    ConnectEngineSystems(engine, &scene, &physicsWorld, &audioEngine, &scriptEngine, &camera);
    
    // Now you can use the public API:
    EntityId car = engine->SpawnEntity("car_sport", {0, 1, 0});
    engine->PlaySound("engine_start.wav");
    
    // ... main loop ...
    
    ShutdownEngine();
    return 0;
}
```

## What Gets Connected

| System | Real Class | How to Access |
|--------|-----------|---------------|
| Scene | `::Scene` | `engine->GetScene()` or `engine->GetRegistry()` |
| Physics | `::PhysicsWorld` | `engine->GetPhysics()` |
| Audio | `::AudioEngine` | `engine->GetAudio()` |
| Scripting | `::ScriptEngine` | `engine->GetScripting()` |
| Rendering | `::Camera` | `engine->GetCameraPosition()` etc |

## Example: Call From Game Code

Once connected, your game code can use the public API without knowing about the internal implementations:

```cpp
// game/GameLogic.cpp
#include <engine/EnginePublic.h>

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

## Current Dummy Systems

If a system isn't connected, the engine uses dummy implementations that:
- Don't crash
- Return sensible defaults
- Log warnings

This allows you to gradually integrate each system:

```cpp
// Phase 1: Connect one system at a time
ConnectEngineSystems(engine, &scene, nullptr, nullptr, nullptr, nullptr);

// Phase 2: Add more as they're tested
ConnectEngineSystems(engine, &scene, &physicsWorld, nullptr, nullptr, nullptr);

// Phase 3: Complete integration
ConnectEngineSystems(engine, &scene, &physicsWorld, &audioEngine, &scriptEngine, &camera);
```

## Next Steps

1. **Locate your main()** - Find where all engine systems are created
2. **Add includes** - Add `#include <engine/EnginePublic.h>`
3. **Call ConnectEngineSystems()** - Pass your real system pointers
4. **Test** - Call `engine->SpawnEntity()` or other API methods
5. **Iterate** - Gradually replace hardcoded logic with public API calls

## Troubleshooting

### "Entity not spawning"
- Check: Is prefab loaded? `GetPrefabManager().HasPrefab("name")`
- Check: Is scene connected? `engine->GetScene()` should not throw
- Check: Is entity valid? `engine->IsEntityValid(id)` should return true

### "Cant access scene registry"
- Ensure Scene pointer is passed to ConnectEngineSystems()
- GetRegistry() will throw if not connected

### "Dummy systems being used"
- Check if pointer was passed to ConnectEngineSystems()
- Use `engine->IsDeveloperMode()` to add debug logging
- Pointer should be non-null to use real system

## Architecture Notes

The public API uses:
- **void* pointers** to avoid circular includes
- **Dummy implementations** for missing systems
- **Lazy initialization** - systems only needed when used
- **Getter functions** - Cast to real types only when needed

This allows clean separation: `/src/` has zero knowledge of `/src/engine/public/`

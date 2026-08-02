# Game Development Guide

This folder contains the **game layer** - the code that developers modify to build their game using the VA Engine.

## ✅ Do Modify (in /game/)
- `main.cpp` - Game entry point
- `GameLogic.cpp/h` - All game logic and behavior
- Add any game-specific systems (AI, quests, dialogue, etc.)
- Create game-specific prefabs and configs in `/assets/`

## ❌ Never Modify (in /src/)
- Engine source code
- Physics system
- Rendering pipeline
- Audio engine
- Scene manager
- Any `src/**` files

The engine code is maintained separately and updated independently.

---

## Basic Game Development Workflow

### 1. Initialize Game
```cpp
#include <engine/EnginePublic.h>

IEngine* engine = InitializeEngine("config/engine.yaml");
```

### 2. Load Scene
```cpp
engine->LoadScene("scenes/my_level.json");
```

### 3. Spawn Entities
```cpp
EntityId player = engine->SpawnEntity("player_character", Vec3(0, 1, 0));
EntityId car = engine->SpawnEntity("car_sport", Vec3(10, 1, 0));
```

### 4. Handle Input
```cpp
InputState input = engine->GetInput();
if (engine->IsKeyHeld(KeyCode::W)) {
    // Move forward
}
```

### 5. Use Events
```cpp
engine->GetEvents().Subscribe(Events::OnCollisionEnter, [](const void* data) {
    EntityId* entities = (EntityId*)data;
    // Handle collision
});
```

---

## File Structure

```
/game/                      ← Your game layer
    main.cpp                ← Entry point
    GameLogic.cpp/.h        ← Main game controller
    Systems/                ← Create your own systems
        PlayerController.cpp
        AISystem.cpp
        QuestSystem.cpp
        DialogueSystem.cpp

/assets/                    ← Data and configs
    scenes/
        level1.json
    prefabs/
        car_sport.json
        npc_pedestrian.json
    config/
        game.yaml           ← Game settings

/src/                       ← Engine code (READ-ONLY)
    /engine/
        /public/            ← Public API for game developers
            EngineAPI.h     ← ONLY include this!
            Types.h
            Scene.h
            etc.
```

---

## Using the Engine API

### Accessing Systems
```cpp
IEngine* engine = GetEngine();

// Rendering
engine->GetRender().SetCameraPosition(Vec3(0, 5, -10));

// Physics
PhysicsWorld& physics = engine->GetPhysics();
RaycastHit hit = physics.Raycast(origin, direction);

// Audio
AudioEngine& audio = engine->GetAudio();
audio.PlayClip(clipId, &position);

// Scripting
engine->GetScripting().ExecuteScript("scripts/game.lua");

// Events
engine->GetEvents().Subscribe(Events::OnEntitySpawned, callback);
```

### Recommended Pattern: System-Based Architecture

```cpp
// Example: PlayerController system
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

## Next Steps

1. **Modify GameLogic.cpp** to add your game mechanics
2. **Create prefabs** in `/assets/prefabs/` 
3. **Add game systems** in `/game/Systems/`
4. **Create Lua scripts** for hot-reloadable game logic
5. **Config-drive everything** - use `/assets/config/` files


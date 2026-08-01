# Engine Architecture for Game Development

## Overview

The VA Engine has been restructured to **separate the engine layer from the game layer**. This means:

✅ **Developers write ONLY in `/game/` and `/assets/`**  
❌ **Developers NEVER modify `/src/` (engine code)**

This separation enables:
- **Easier updates** - Engine can be updated independently
- **Cleaner code** - Game code is isolated from engine internals
- **No recompilation** - Use Lua scripts and config files instead
- **Rapid iteration** - Hot-reload for instant feedback
- **Reusability** - Engine can be used for multiple game projects

---

## What Was Added (1-by-1)

### 1. ✅ Public Engine API Headers (`/src/engine/public/`)

**Purpose:** Define the interface that game developers use

**Files Created:**
- `Types.h` - Core types (Vec3, Transform, RaycastHit, etc.)
- `EngineAPI.h` - Main engine interface (IEngine)
- `EventSystem.h` - Event pub/sub interface
- `Scene.h` - Scene management interface
- `PhysicsWorld.h` - Physics queries interface
- `AudioEngine.h` - Audio playback interface
- `InputSystem.h` - Input handling interface
- `ScriptingEngine.h` - Lua scripting interface
- `RenderSystem.h` - Rendering interface
- `EnginePublic.h` - Includes all public headers

**Key Idea:** Game code only includes `EnginePublic.h`, never internal engine headers.

---

### 2. ✅ Game Layer Separation (`/game/`)

**Purpose:** Template structure for game developers

**Files Created:**
- `main.cpp` - Game entry point, clean separation from engine
- `GameLogic.h/cpp` - Main game controller class
- `README.md` - Developer guide

**Example Flow:**
```
Engine (C++) runs main loop
  ↓
Calls game->Update(deltaTime)
  ↓
GameLogic uses EngineAPI to control game
  ↓
No modification to /src/ needed
```

---

### 3. ✅ Event/Messaging System (`/src/engine/EventSystem.cpp`)

**Purpose:** Decouple engine from game via pub/sub events

**Features:**
- Subscribe to events by name: `engine->GetEvents().Subscribe("OnEntityDestroyed", callback)`
- Emit events: `engine->GetEvents().Emit("OnGameStarted")`
- Built-in events: OnEntitySpawned, OnCollisionEnter, OnGamePaused, etc.

**Benefit:** Game logic reacts to events without hardcoding into engine

---

### 4. ✅ Lua API Bindings (`/src/engine/LuaBindings.cpp/h`)

**Purpose:** Call engine functions from Lua scripts

**Registered Functions:**
```lua
-- Entity management
local car = Engine:SpawnEntity("car_sport", {x=0, y=1, z=0})
Engine:DestroyEntity(car)

-- Physics
local hit = Engine:Raycast({x=0, y=0, z=0}, {x=0, y=-1, z=0}, 100)

-- Input
if Engine:IsKeyHeld(KeyCode.W) then
    -- Move forward
end

-- Events
Engine:Subscribe("OnGameStarted", function()
    print("Game started!")
end)

-- Audio
Engine:PlaySound("audio/explosion.wav", {x=0, y=0, z=0})
```

**Benefit:** Developers can write game logic in Lua without C++ recompilation

---

### 5. ✅ Configuration System (`/src/engine/Config.cpp/h`)

**Purpose:** Store all tunable values in JSON files

**Usage:**
```cpp
GetConfig().LoadFromFile("assets/config/game.json");
float renderDist = GetConfig().GetFloat("rendering.renderDistance", 500.0f);

// Hot reload
if (GetConfig().HasChanged()) {
    GetConfig().Reload();
}
```

**Example Config:**
```json
{
  "game": {"maxPlayers": 32, "difficulty": "normal"},
  "rendering": {"renderDistance": 500, "quality": "high"},
  "physics": {"gravity": [0, -9.81, 0]}
}
```

**Benefit:** Tune game parameters without recompiling

---

### 6. ✅ Prefab System (`/src/engine/Prefab.cpp/h`)

**Purpose:** Define entity templates in JSON

**Usage:**
```cpp
GetPrefabManager().LoadPrefabsFromDirectory("assets/prefabs/");
EntityId car = GetPrefabManager().Instantiate("car_sport", position, &scene);
```

**Example Prefab:**
```json
{
  "name": "car_sport",
  "components": {
    "Transform": {"position": [0, 1, 0]},
    "Model": {"meshPath": "models/car.gltf"},
    "Physics": {"mass": 1500, "type": "Dynamic"},
    "Vehicle": {"maxSpeed": 250},
    "Script": {"mainScript": "scripts/vehicle.lua"}
  }
}
```

**Benefit:** Create complex entities without C++ code

---

### 7. ✅ Hot-Reload System (`/src/engine/HotReload.cpp/h`)

**Purpose:** Auto-reload files when they change on disk

**Usage:**
```cpp
GetHotReloadManager().Watch("game/scripts/game.lua", [engine](const std::string& path) {
    engine->ReloadScript(path);
});

// In main loop
if (engine->IsDeveloperMode()) {
    GetHotReloadManager().CheckForChanges();  // Detects file changes
}
```

**Benefit:** Edit Lua scripts, config files, and see changes instantly without restart

---

## File Structure Overview

```
VA Engine/
├── src/                          ← Engine code (READ-ONLY for developers)
│   ├── engine/
│   │   ├── public/               ← Public API headers (game code includes these)
│   │   │   ├── EnginePublic.h
│   │   │   ├── Types.h
│   │   │   ├── EngineAPI.h
│   │   │   ├── EventSystem.h
│   │   │   └── ... (other interfaces)
│   │   ├── EventSystem.cpp       ← Engine implementations
│   │   ├── Engine.cpp
│   │   ├── LuaBindings.cpp
│   │   ├── Config.cpp
│   │   ├── Prefab.cpp
│   │   └── HotReload.cpp
│   ├── main.cpp                  ← OLD: Don't use anymore
│   ├── rendering/                ← Engine systems
│   ├── physics/
│   ├── audio/
│   ├── scene/
│   └── ... (other engine code)
│
├── game/                         ← Game code (DEVELOPERS MODIFY THIS)
│   ├── main.cpp                  ← Game entry point
│   ├── GameLogic.h/cpp           ← Main game controller
│   ├── systems/                  ← Add your game systems here
│   │   ├── PlayerController.cpp
│   │   ├── AISystem.cpp
│   │   └── QuestSystem.cpp
│   ├── scripts/                  ← Lua scripts
│   │   └── game.lua
│   └── README.md
│
├── assets/                       ← Data (DEVELOPERS MODIFY THIS)
│   ├── config/                   ← Configuration files
│   │   ├── game.json             ← Game settings
│   │   ├── engine.yaml           ← Engine settings
│   │   └── README.md
│   ├── prefabs/                  ← Entity templates
│   │   ├── car_sport.json
│   │   ├── npc_pedestrian.json
│   │   └── README.md
│   ├── scenes/                   ← Level/scene files
│   ├── models/                   ← 3D models
│   ├── scripts/                  ← More Lua scripts
│   ├── audio/                    ← Sounds
│   └── textures/                 ← Images
│
└── CMakeLists.txt
```

---

## How to Use It

### For Beginners: Start with GameLogic

1. **Open** `game/GameLogic.cpp`
2. **Modify** `HandleInput()` to respond to player input
3. **Modify** `UpdatePlayer()` to move entities
4. **Run** - Changes compile and run immediately

### For Advanced: Use Lua Scripts

1. **Create** `game/scripts/my_system.lua`
2. **Edit** to add game logic
3. **Save** - Script reloads automatically (no restart!)
4. **Test** instantly

### For Data: Use Prefabs & Config

1. **Create** `assets/prefabs/my_entity.json`
2. **Define** components and properties
3. **Spawn** via: `Engine:SpawnEntity("my_entity", position)`
4. **No recompilation needed**

---

## Key Benefits

| Feature | Benefit |
|---------|---------|
| Public API | Game code isolated from engine internals |
| Lua Bindings | Script game logic without C++ recompilation |
| Events | Loose coupling between systems |
| Config Files | Tune parameters without code changes |
| Prefabs | Create entities via JSON, not C++ |
| Hot-Reload | Edit and see changes instantly |

---

## Next Steps for Developers

1. **Read** `/game/README.md` for development guide
2. **Start coding** in `game/GameLogic.cpp`
3. **Add systems** in `game/systems/` as needed
4. **Use Lua** for rapid iteration (`game/scripts/`)
5. **Configure** via JSON files in `assets/config/`
6. **Define entities** as prefabs in `assets/prefabs/`

---

## For Engine Developers (Maintaining VA Engine)

If you need to **update the engine itself**:

1. Modify files in `/src/engine/public/` for API changes
2. Implement in `/src/engine/*.cpp` 
3. **Never** expose internal structs to public API
4. Update `LuaBindings.cpp` if adding new functions
5. Increment version if breaking changes

---

## Summary

✅ **Done:** 7-layer separation of concerns  
✅ **Engine** is maintainable and reusable  
✅ **Game developers** have clear, documented paths  
✅ **Rapid iteration** via Lua + hot-reload  
✅ **Data-driven** via prefabs + config  

**Ready to start building your game!** 🎮

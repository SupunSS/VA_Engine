# Prefab System

Prefabs are reusable entity templates defined in JSON files. They allow you to create complex entities without writing C++ code.

## Quick Start

### 1. Create a Prefab

Create `assets/prefabs/my_entity.json`:

```json
{
  "name": "my_entity",
  "description": "My custom entity type",
  "components": {
    "Transform": {
      "position": [0, 0, 0],
      "rotation": [0, 0, 0, 1],
      "scale": [1, 1, 1]
    },
    "Model": {
      "meshPath": "models/my_model.gltf",
      "materialPath": "materials/my_material.yaml"
    },
    "Physics": {
      "type": "Dynamic",
      "mass": 100,
      "drag": 0.1
    },
    "Script": {
      "mainScript": "scripts/my_entity.lua",
      "scriptData": {
        "speed": 10,
        "health": 100
      }
    }
  }
}
```

### 2. Spawn Prefab

In C++:
```cpp
#include <engine/Prefab.h>

Vec3 position(10, 1, 5);
EntityId entity = GetPrefabManager().Instantiate("my_entity", position, &scene);
```

In Lua:
```lua
local entity = Engine:SpawnEntity("my_entity", {x=10, y=1, z=5})
```

## Prefab Structure

### Root Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | ✓ | Unique identifier for prefab |
| `description` | string | | Human-readable description |
| `components` | object | ✓ | Component definitions |
| `metadata` | object | | Custom metadata (author, version, etc.) |

### Components

#### Transform
```json
"Transform": {
  "position": [x, y, z],
  "rotation": [x, y, z, w],  // Quaternion
  "scale": [x, y, z]
}
```

#### Model
```json
"Model": {
  "meshPath": "models/mesh.gltf",
  "materialPath": "materials/material.yaml",
  "visible": true
}
```

#### Physics
```json
"Physics": {
  "type": "Dynamic",           // Static, Dynamic, Kinematic
  "mass": 100,
  "drag": 0.3,
  "angularDrag": 0.05,
  "useGravity": true,
  "isCollider": true
}
```

#### Vehicle
```json
"Vehicle": {
  "type": "car",              // car, truck, bike, etc.
  "maxSpeed": 250,
  "acceleration": 20,
  "steerSensitivity": 1.5,
  "suspensionStiffness": 50
}
```

#### Animation
```json
"Animation": {
  "skeletonPath": "models/skeleton.gltf",
  "animations": {
    "idle": "anims/idle.gltf",
    "walk": "anims/walk.gltf",
    "run": "anims/run.gltf"
  }
}
```

#### Script
```json
"Script": {
  "mainScript": "scripts/my_script.lua",
  "scriptData": {
    "param1": "value1",
    "param2": 42
  }
}
```

#### Audio
```json
"Audio": {
  "audioClips": {
    "footstep": "audio/sfx/footstep.wav",
    "death": "audio/sfx/death.wav"
  },
  "volume": 0.8
}
```

#### AI
```json
"AI": {
  "type": "Pedestrian",
  "speed": 1.5,
  "walkRadius": 30,
  "idleTime": 3,
  "behaviour": "patrol"
}
```

## Usage Examples

### Simple Prop
```json
{
  "name": "barrel_fuel",
  "components": {
    "Transform": {"position": [0, 0, 0]},
    "Model": {"meshPath": "models/barrel.gltf"},
    "Physics": {"type": "Dynamic", "mass": 50}
  }
}
```

### Character with Animation
```json
{
  "name": "zombie",
  "components": {
    "Transform": {"position": [0, 0, 0]},
    "Model": {"meshPath": "models/zombie.gltf"},
    "Physics": {"type": "Dynamic", "mass": 80},
    "Animation": {
      "skeletonPath": "models/zombie_skeleton.gltf",
      "animations": {
        "idle": "anims/zombie_idle.gltf",
        "walk": "anims/zombie_walk.gltf",
        "attack": "anims/zombie_attack.gltf"
      }
    },
    "AI": {"type": "Zombie", "speed": 2.0},
    "Script": {"mainScript": "scripts/zombie_ai.lua"}
  }
}
```

### Interactive Object
```json
{
  "name": "door",
  "components": {
    "Transform": {"position": [0, 1, 0]},
    "Model": {"meshPath": "models/door.gltf"},
    "Physics": {"type": "Kinematic"},
    "Script": {
      "mainScript": "scripts/door.lua",
      "scriptData": {"locked": false, "openTime": 0.5}
    },
    "Audio": {
      "audioClips": {"open": "audio/door_open.wav"}
    }
  }
}
```

## Prefab Management

### Load Prefabs
```cpp
// Load single prefab
GetPrefabManager().LoadPrefab("assets/prefabs/car.json");

// Load all prefabs from directory
int count = GetPrefabManager().LoadPrefabsFromDirectory("assets/prefabs/");
```

### Check and Get
```cpp
if (GetPrefabManager().HasPrefab("car_sport")) {
    const Prefab* prefab = GetPrefabManager().GetPrefab("car_sport");
    // Use prefab data...
}
```

### List All
```cpp
auto names = GetPrefabManager().GetPrefabNames();
for (const auto& name : names) {
    std::cout << "Prefab: " << name << std::endl;
}
```

### Unload
```cpp
// Unload single
GetPrefabManager().UnloadPrefab("car_sport");

// Unload all
GetPrefabManager().UnloadAll();
```

## Best Practices

1. **Use meaningful names** - `car_sport_red` not just `car`
2. **Organize by category** - Put similar prefabs in subdirectories
3. **Document components** - Use descriptive field names and add comments
4. **Provide defaults** - Include reasonable default values for all fields
5. **Don't hardcode** - Use prefabs instead of creating entities in C++
6. **Version your prefabs** - Consider a `version` field for tracking changes
7. **Test spawning** - Verify each prefab spawns correctly
8. **Reuse components** - Extract common patterns into base prefabs (inheritance via includes)

## Prefab Inheritance (Future)

```json
{
  "name": "enemy_zombie_fast",
  "inherits": "zombie",
  "components": {
    "AI": {"speed": 3.5}
  }
}
```

## Variants (Future)

```json
{
  "name": "zombie",
  "variants": {
    "damaged": {"Physics": {"mass": 60}},
    "burning": {"Audio": {"audioClips": {"burn": "audio/fire.wav"}}}
  }
}
```

## Next: Streaming & LOD

Prefabs should support:
- Level-of-detail (LOD) variants
- Streaming load/unload
- Pooling and reuse
- Performance profiling per prefab

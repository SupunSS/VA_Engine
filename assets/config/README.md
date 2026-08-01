# Configuration System

The configuration system allows developers to tune engine and game parameters without recompiling code.

## Files

- `assets/config/engine.yaml` - Engine settings
- `assets/config/game.json` - Game-specific settings
- `assets/config/input.json` - Input key bindings (future)

## Usage

### Loading Configuration

```cpp
#include <engine/Config.h>

// Load configuration
GetConfig().LoadFromFile("assets/config/game.json");

// Get values
float renderDist = GetConfig().GetFloat("rendering.renderDistance", 500.0f);
bool fullscreen = GetConfig().GetBool("rendering.fullscreen", false);
int maxPlayers = GetConfig().GetInt("gameplay.maxPlayers", 1);
std::string title = GetConfig().GetString("game.title", "Game");
```

### Runtime Changes

```cpp
// Change value at runtime
GetConfig().Set("rendering.renderDistance", 750.0f);
GetConfig().Set("gameplay.difficulty", "hard");
```

### Hot-Reload

```cpp
// Check if file was changed on disk
if (GetConfig().HasChanged()) {
    GetConfig().Reload();
    // Reapply settings...
}
```

### Lua Access

```lua
-- Expose config to Lua (requires binding implementation)
local renderDist = Engine:GetConfig("rendering.renderDistance")
Engine:SetConfig("rendering.renderDistance", 750)
```

## Configuration File Format

### JSON (game.json)
```json
{
  "game": {
    "title": "My Game",
    "difficulty": "normal"
  },
  "rendering": {
    "renderDistance": 500,
    "quality": "high"
  }
}
```

### YAML (engine.yaml)
```yaml
engine:
  targetFPS: 60
  vsync: true
rendering:
  resolution:
    width: 1920
    height: 1080
```

## Dot-Notation Access

Use dots to access nested values:

```cpp
// "rendering.quality" accesses: config["rendering"]["quality"]
GetConfig().GetString("rendering.quality");

// "gameplay.player.maxHealth" accesses: config["gameplay"]["player"]["maxHealth"]
GetConfig().GetInt("gameplay.player.maxHealth");
```

## Common Configuration Keys

### Rendering
- `rendering.renderDistance` - Maximum draw distance
- `rendering.quality` - Quality preset (low, medium, high, ultra)
- `rendering.vsync` - Enable/disable vertical sync
- `rendering.antiAliasing` - AA method (none, FXAA, MSAA)
- `rendering.resolution.width` - Screen width
- `rendering.resolution.height` - Screen height

### Physics
- `physics.gravity` - Gravity acceleration
- `physics.timeStep` - Physics update rate
- `physics.maxBodies` - Maximum physics bodies

### Audio
- `audio.masterVolume` - Master volume 0-1
- `audio.musicVolume` - Music volume 0-1
- `audio.sfxVolume` - SFX volume 0-1

### Gameplay
- `gameplay.difficulty` - Game difficulty
- `gameplay.maxPlayers` - Maximum players
- `gameplay.playerHealthPoints` - Player starting health

### Debug
- `debug.showFPS` - Show FPS counter
- `debug.showPhysicsDebug` - Show physics debug visuals
- `debug.logLevel` - Logging level (debug, info, warning, error)

## Best Practices

1. **Always provide defaults** - Use the second parameter to `Get()` for fallback values
2. **Organize by section** - Use dot-notation to organize related settings
3. **Use meaningful names** - Be descriptive: `rendering.shadowQuality` not `shq`
4. **Hot-reload in dev** - Check `HasChanged()` in debug builds
5. **Validate values** - Clamp/validate important parameters after loading
6. **Document settings** - Add comments in config files explaining each setting
7. **Version configs** - Consider adding a `config_version` field to track breaking changes

## Next: Input Configuration

The input system should also use config:
```json
{
  "input": {
    "moveForward": "W",
    "moveBackward": "S",
    "turnLeft": "A",
    "turnRight": "D"
  }
}
```

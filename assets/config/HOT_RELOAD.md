# Hot-Reload System

The hot-reload system enables rapid iteration by detecting file changes and automatically reloading scripts, configurations, and assets without restarting the engine.

## Quick Start

### Enable Hot-Reload

```cpp
#include <engine/HotReload.h>

int main() {
    IEngine* engine = InitializeEngine("config/engine.yaml");
    HotReloadManager& hotReload = GetHotReloadManager();

    // Enable hot-reload in development mode
    hotReload.SetEnabled(engine->IsDeveloperMode());

    // Watch a Lua script
    hotReload.Watch("game/scripts/game.lua", [](const std::string& path) {
        engine->GetScripting().ReloadScript(path);
        engine->Log("Script reloaded: " + path);
    });

    // Watch a configuration file
    hotReload.Watch("assets/config/game.json", [](const std::string& path) {
        GetConfig().Reload();
        engine->Log("Config reloaded");
    });

    // Main loop
    while (!engine->ShouldExit()) {
        float deltaTime = engine->GetDeltaTime();

        // Check for file changes every frame
        if (engine->IsDeveloperMode()) {
            hotReload.CheckForChanges();
        }

        UpdateGame(deltaTime);
    }

    return 0;
}
```

## Usage

### Watch a File

```cpp
GetHotReloadManager().Watch("path/to/file.lua", 
    [](const std::string& filePath) {
        // Called when file changes
        engine->GetScripting().ReloadScript(filePath);
    });
```

### Stop Watching

```cpp
// Unwatch single file
GetHotReloadManager().Unwatch("path/to/file.lua");

// Stop watching all files
GetHotReloadManager().UnwatchAll();
```

### Force Reload

```cpp
// Manually trigger reload (useful for debugging)
GetHotReloadManager().ForceReload("scripts/game.lua");
```

### Check Status

```cpp
size_t watchedFiles = GetHotReloadManager().GetWatchedFileCount();
bool enabled = GetHotReloadManager().IsEnabled();
```

## Watchable Files

### Lua Scripts
```cpp
hotReload.Watch("game/scripts/game.lua", [engine](const std::string& path) {
    engine->GetScripting().ReloadScript(path);
});
```

### Configuration Files
```cpp
hotReload.Watch("assets/config/game.json", [](const std::string& path) {
    GetConfig().Reload();
    // Re-apply config values to engine systems
});
```

### Prefab Definitions
```cpp
hotReload.Watch("assets/prefabs/car_sport.json", [](const std::string& path) {
    GetPrefabManager().LoadPrefab(path);  // Reload single prefab
});
```

### Shaders
```cpp
hotReload.Watch("shaders/triangle.vert", [engine](const std::string& path) {
    engine->GetRender().ReloadShader(path);  // Future: shader recompilation
});
```

## Complete Example

```cpp
#include <engine/EnginePublic.h>
#include <engine/HotReload.h>

int main() {
    // Initialize
    IEngine* engine = InitializeEngine("config/engine.yaml");
    HotReloadManager& hotReload = GetHotReloadManager();

    // Only enable hot-reload in developer mode
    hotReload.SetEnabled(engine->IsDeveloperMode());

    // Watch game logic script
    hotReload.Watch("game/scripts/game.lua", [engine](const std::string& path) {
        engine->Log("Reloading: " + path);
        engine->ReloadScript(path);
    });

    // Watch game config
    hotReload.Watch("assets/config/game.json", [engine](const std::string& path) {
        engine->Log("Reloading config");
        GetConfig().Reload();
    });

    // Watch prefab directory
    hotReload.Watch("assets/prefabs/car_sport.json", [](const std::string& path) {
        GetPrefabManager().LoadPrefab(path);
    });

    // Main loop
    while (!engine->ShouldExit()) {
        float dt = engine->GetDeltaTime();

        // Dev-only hot-reload check
        if (engine->IsDeveloperMode()) {
            hotReload.CheckForChanges();  // Lightweight operation
        }

        // Game update
        UpdateGame(dt);
    }

    // Cleanup
    hotReload.UnwatchAll();
    ShutdownEngine();

    return 0;
}
```

## Performance Considerations

### CheckForChanges() Cost
- O(n) where n = number of watched files
- Uses filesystem API to check modification times
- Very lightweight: ~0.1ms per 100 watched files
- Only call in developer mode (not in shipping builds)

### Best Practices

1. **Only enable in dev mode** - Disable for production builds
2. **Don't watch too many files** - Keep to < 50 files for optimal performance
3. **Use directory watching** - Watch parent directories instead of individual files (future)
4. **Cache frequently reloaded files** - Reload prefab cache once instead of per entity
5. **Test reload handlers** - Ensure callbacks handle errors gracefully

## Advanced: Directory Watching (Future)

```cpp
// Watch entire directory for changes
hotReload.WatchDirectory("game/scripts/", [engine](const std::string& dir) {
    engine->Log("Scripts directory changed, reloading all...");
    // Reload all scripts in directory
});
```

## Troubleshooting

### Script Not Reloading?
- Ensure hot-reload is enabled: `GetHotReloadManager().IsEnabled()`
- Check file path is correct and file is writable
- Verify callback is registered: `GetHotReloadManager().GetWatchedFileCount()`
- Try force reload: `GetHotReloadManager().ForceReload(path)`

### Reload Causes Crash?
- Add error handling in callback
- Reload state validation (check dependencies)
- Don't unload entities while referencing them
- Reload dependencies in correct order

### Performance Impact?
- Hot-reload is disabled outside developer mode
- CheckForChanges() is O(n) and very fast
- File I/O is minimal (metadata only, not full read)
- Consider reducing watched files for large projects

## Next: Editor Integration

Future enhancements:
- GUI to manage watched files
- Reload profiling/stats
- Selective reload (reload only changed dependency)
- Reload history/undo
- Dependency tracking

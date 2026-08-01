/**
 * Hot-Reload System
 * 
 * Monitors files for changes and reloads scripts/configs automatically.
 * Essential for rapid iteration during development.
 * 
 * Example:
 *   Modify scripts/game.lua on disk
 *   Engine automatically detects change
 *   Script is reloaded without restart
 *   Game continues running
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>

/**
 * File watcher callback
 * Called when a watched file changes
 */
using FileChangeCallback = std::function<void(const std::string& filePath)>;

/**
 * Hot-reload manager
 * 
 * Usage:
 *   HotReloadManager& mgr = GetHotReloadManager();
 *   
 *   // Watch a file
 *   mgr.Watch("scripts/game.lua", [](const std::string& path) {
 *       Engine.GetScripting().ReloadScript(path);
 *   });
 *   
 *   // In update loop
 *   mgr.CheckForChanges();
 */
class HotReloadManager {
public:
    /**
     * Enable/disable hot reload globally
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    /**
     * Watch a file for changes
     * @param filePath Path to file to watch
     * @param callback Function to call when file changes
     */
    void Watch(const std::string& filePath, FileChangeCallback callback);

    /**
     * Stop watching a file
     * @param filePath Path to file to stop watching
     */
    void Unwatch(const std::string& filePath);

    /**
     * Stop watching all files
     */
    void UnwatchAll();

    /**
     * Check if any watched files have changed
     * Called from main loop (expensive operation)
     */
    void CheckForChanges();

    /**
     * Get number of watched files
     */
    size_t GetWatchedFileCount() const { return m_watchers.size(); }

    /**
     * Manual trigger reload for a file
     * Useful for forcing reload without waiting for file change
     */
    void ForceReload(const std::string& filePath);

private:
    struct FileWatcher {
        std::string filePath;
        uint64_t lastModifiedTime;
        FileChangeCallback callback;
    };

    bool m_enabled = true;
    std::vector<FileWatcher> m_watchers;

    /**
     * Get file modification time
     */
    uint64_t GetFileModTime(const std::string& filePath) const;
};

// ============================================================================
// GLOBAL HOT-RELOAD MANAGER
// ============================================================================

/**
 * Get the global hot-reload manager
 */
extern HotReloadManager& GetHotReloadManager();

/**
 * Example usage in engine main loop:
 * 
 *   while (!engine->ShouldExit()) {
 *       // Check for file changes
 *       if (engine->IsDeveloperMode()) {
 *           GetHotReloadManager().CheckForChanges();
 *       }
 *       
 *       // Game update
 *       gameLogic->Update(deltaTime);
 *   }
 */

/**
 * Hot-Reload System Implementation
 */

#include "HotReload.h"
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

void HotReloadManager::Watch(const std::string& filePath, FileChangeCallback callback) {
    if (!m_enabled) {
        return;
    }

    // Check if already watching
    for (auto& watcher : m_watchers) {
        if (watcher.filePath == filePath) {
            watcher.callback = callback;
            return;
        }
    }

    // Add new watcher
    FileWatcher watcher;
    watcher.filePath = filePath;
    watcher.lastModifiedTime = GetFileModTime(filePath);
    watcher.callback = callback;

    m_watchers.push_back(watcher);
    std::cout << "HotReload: Watching " << filePath << std::endl;
}

void HotReloadManager::Unwatch(const std::string& filePath) {
    auto it = std::find_if(m_watchers.begin(), m_watchers.end(),
        [&](const FileWatcher& w) { return w.filePath == filePath; });

    if (it != m_watchers.end()) {
        m_watchers.erase(it);
        std::cout << "HotReload: Stopped watching " << filePath << std::endl;
    }
}

void HotReloadManager::UnwatchAll() {
    m_watchers.clear();
    std::cout << "HotReload: Stopped watching all files" << std::endl;
}

void HotReloadManager::CheckForChanges() {
    if (!m_enabled) {
        return;
    }

    for (auto& watcher : m_watchers) {
        uint64_t currentModTime = GetFileModTime(watcher.filePath);

        if (currentModTime != 0 && currentModTime != watcher.lastModifiedTime) {
            std::cout << "HotReload: File changed: " << watcher.filePath << std::endl;
            watcher.lastModifiedTime = currentModTime;

            if (watcher.callback) {
                try {
                    watcher.callback(watcher.filePath);
                }
                catch (const std::exception& e) {
                    std::cerr << "HotReload: Error in callback for " << watcher.filePath
                             << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void HotReloadManager::ForceReload(const std::string& filePath) {
    for (auto& watcher : m_watchers) {
        if (watcher.filePath == filePath) {
            std::cout << "HotReload: Force reloading " << filePath << std::endl;
            watcher.lastModifiedTime = 0;  // Reset so next check triggers reload

            if (watcher.callback) {
                try {
                    watcher.callback(filePath);
                }
                catch (const std::exception& e) {
                    std::cerr << "HotReload: Error in callback for " << filePath
                             << ": " << e.what() << std::endl;
                }
            }
            return;
        }
    }
}

uint64_t HotReloadManager::GetFileModTime(const std::string& filePath) const {
    try {
        if (fs::exists(filePath)) {
            auto writeTime = fs::last_write_time(filePath);
            return writeTime.time_since_epoch().count();
        }
    }
    catch (...) {
        // File doesn't exist or error reading
    }

    return 0;
}

// ============================================================================
// GLOBAL HOT-RELOAD MANAGER
// ============================================================================

static HotReloadManager g_hotReloadManager;

HotReloadManager& GetHotReloadManager() {
    return g_hotReloadManager;
}

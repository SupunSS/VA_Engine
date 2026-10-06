/**
 * Configuration System
 * 
 * Loads and manages configuration from JSON/YAML files.
 * Allows developers to tune game parameters without recompiling.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Configuration manager
 * 
 * Usage:
 *   ConfigManager config;
 *   config.LoadFromFile("config/game.json");
 *   
 *   float renderDist = config.Get<float>("rendering.renderDistance", 500.0f);
 *   bool fullscreen = config.GetBool("rendering.fullscreen", false);
 *   
 *   // Hot-reload
 *   if (config.HasChanged()) {
 *       config.Reload();
 *   }
 */
class ConfigManager {
public:
    /**
     * Load configuration from file
     * @param filePath Path to JSON/YAML config file
     */
    void LoadFromFile(const std::string& filePath);

    /**
     * Reload configuration from last loaded file
     */
    void Reload();

    /**
     * Get a configuration value
     * @param key Dot-separated key path (e.g., "rendering.renderDistance")
     * @param defaultValue Value to return if key not found
     */
    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T()) const;

    /**
     * Get a boolean value
     * @param key Dot-separated key path
     * @param defaultValue Default value if not found
     */
    bool GetBool(const std::string& key, bool defaultValue = false) const;

    /**
     * Get a string value
     * @param key Dot-separated key path
     * @param defaultValue Default value if not found
     */
    std::string GetString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * Get an integer value
     * @param key Dot-separated key path
     * @param defaultValue Default value if not found
     */
    int GetInt(const std::string& key, int defaultValue = 0) const;

    /**
     * Get a floating point value
     * @param key Dot-separated key path
     * @param defaultValue Default value if not found
     */
    float GetFloat(const std::string& key, float defaultValue = 0.0f) const;

    /**
     * Set a configuration value (at runtime)
     * @param key Dot-separated key path
     * @param value New value
     */
    template<typename T>
    void Set(const std::string& key, const T& value);

    /**
     * Check if configuration file has been modified on disk
     * @return true if file was changed
     */
    bool HasChanged() const;

    /**
     * Get the raw JSON object for advanced usage
     * @return Reference to JSON data
     */
    const json& GetJson() const { return m_data; }

    /**
     * Get full path to loaded config file
     * @return File path
     */
    const std::string& GetFilePath() const { return m_filePath; }

private:
    json m_data;
    std::string m_filePath;
    uint64_t m_lastModifiedTime = 0;

    /**
     * Navigate JSON object with dot-separated path
     */
    json* FindValue(const std::string& key);
    const json* FindValueConst(const std::string& key) const;

    /**
     * Get file modification time
     */
    uint64_t GetFileModTime(const std::string& filePath) const;
};

// ============================================================================
// GLOBAL CONFIG INSTANCE
// ============================================================================

/**
 * Get the global configuration manager
 */
extern ConfigManager& GetConfig();

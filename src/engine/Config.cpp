/**
 * Configuration System Implementation
 */

#include "Config.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void ConfigManager::LoadFromFile(const std::string& filePath) {
    m_filePath = filePath;

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Config: Failed to open file: " << filePath << std::endl;
            return;
        }

        file >> m_data;
        m_lastModifiedTime = GetFileModTime(filePath);
        
        std::cout << "Config: Loaded from " << filePath << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Config: Error parsing file: " << e.what() << std::endl;
    }
}

void ConfigManager::Reload() {
    LoadFromFile(m_filePath);
}

json* ConfigManager::FindValue(const std::string& key) {
    // Split by dots: "rendering.renderDistance" -> ["rendering", "renderDistance"]
    std::istringstream iss(key);
    std::string part;
    json* current = &m_data;

    while (std::getline(iss, part, '.')) {
        if (current->is_object() && current->contains(part)) {
            current = &(*current)[part];
        } else {
            return nullptr;
        }
    }

    return current;
}

const json* ConfigManager::FindValueConst(const std::string& key) const {
    std::istringstream iss(key);
    std::string part;
    const json* current = &m_data;

    while (std::getline(iss, part, '.')) {
        if (current->is_object() && current->contains(part)) {
            current = &(*current)[part];
        } else {
            return nullptr;
        }
    }

    return current;
}

template<typename T>
T ConfigManager::Get(const std::string& key, const T& defaultValue) const {
    const json* value = FindValueConst(key);
    if (value == nullptr) {
        return defaultValue;
    }

    try {
        return value->get<T>();
    }
    catch (...) {
        return defaultValue;
    }
}

// Explicit template instantiations
template int ConfigManager::Get<int>(const std::string&, const int&) const;
template float ConfigManager::Get<float>(const std::string&, const float&) const;
template std::string ConfigManager::Get<std::string>(const std::string&, const std::string&) const;

bool ConfigManager::GetBool(const std::string& key, bool defaultValue) const {
    return Get<bool>(key, defaultValue);
}

std::string ConfigManager::GetString(const std::string& key, const std::string& defaultValue) const {
    return Get<std::string>(key, defaultValue);
}

int ConfigManager::GetInt(const std::string& key, int defaultValue) const {
    return Get<int>(key, defaultValue);
}

float ConfigManager::GetFloat(const std::string& key, float defaultValue) const {
    return Get<float>(key, defaultValue);
}

template<typename T>
void ConfigManager::Set(const std::string& key, const T& value) {
    std::istringstream iss(key);
    std::string part;
    json* current = &m_data;

    // Navigate to parent object, creating path if needed
    std::vector<std::string> parts;
    while (std::getline(iss, part, '.')) {
        parts.push_back(part);
    }

    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->contains(parts[i])) {
            (*current)[parts[i]] = json::object();
        }
        current = &(*current)[parts[i]];
    }

    // Set the final value
    if (!parts.empty()) {
        (*current)[parts.back()] = value;
    }
}

// Explicit template instantiations
template void ConfigManager::Set<int>(const std::string&, const int&);
template void ConfigManager::Set<float>(const std::string&, const float&);
template void ConfigManager::Set<std::string>(const std::string&, const std::string&);
template void ConfigManager::Set<bool>(const std::string&, const bool&);

bool ConfigManager::HasChanged() const {
    if (m_filePath.empty()) {
        return false;
    }

    return GetFileModTime(m_filePath) != m_lastModifiedTime;
}

uint64_t ConfigManager::GetFileModTime(const std::string& filePath) const {
    try {
        auto writeTime = fs::last_write_time(filePath);
        return writeTime.time_since_epoch().count();
    }
    catch (...) {
        return 0;
    }
}

// ============================================================================
// GLOBAL CONFIG INSTANCE
// ============================================================================

static ConfigManager g_config;

ConfigManager& GetConfig() {
    return g_config;
}

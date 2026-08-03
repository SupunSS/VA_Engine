#include "AssetPaths.h"
#include "../engine/Config.h"
#include <filesystem>
#include <array>

namespace AssetPaths {

namespace {
bool g_loaded = false;

struct CategoryDefault {
    Category category;
    const char* configKey;   // under "paths."
    const char* defaultRoot;
};

// Defaults match this project's actual on-disk layout: flat folders at
// the project root (confirmed against EditorUI's kAssetFolders, and every
// hardcoded path previously found in main.cpp/SceneLoader/SaveSystem) —
// NOT nested under a separate "assets/" parent.
constexpr std::array<CategoryDefault, 9> kDefaults = {{
    { Category::Models,   "paths.models",   "models" },
    { Category::Textures, "paths.textures", "textures" },
    { Category::Audio,    "paths.audio",    "audio" },
    { Category::Scripts,  "paths.scripts",  "game/scripts" },
    { Category::Prefabs,  "paths.prefabs",  "prefabs" },
    { Category::Scenes,   "paths.scenes",   "scenes" },
    { Category::Shaders,  "paths.shaders",  "shaders" },
    { Category::Config,   "paths.config",   "config" },
    { Category::Saves,    "paths.saves",    "saves" },
}};

const CategoryDefault& Lookup(Category category) {
    for (const auto& entry : kDefaults) {
        if (entry.category == category) return entry;
    }
    return kDefaults[0]; // unreachable as long as every enumerator has an entry above
}
} // namespace

void EnsureLoaded() {
    if (g_loaded) return;
    g_loaded = true;
    GetConfig().LoadFromFile("config/engine.yaml");
}

std::string Root(Category category) {
    EnsureLoaded();
    const CategoryDefault& entry = Lookup(category);
    return GetConfig().GetString(entry.configKey, entry.defaultRoot);
}

std::string Resolve(Category category, const std::string& relativePath) {
    if (relativePath.empty()) {
        return relativePath;
    }

    std::filesystem::path asPath(relativePath);
    if (asPath.is_absolute()) {
        return relativePath;
    }

    std::string root = Root(category);
    std::filesystem::path rootPath(root);

    auto relativeToRoot = asPath.lexically_relative(rootPath);
    if (!relativeToRoot.empty()) {
        auto firstPart = relativeToRoot.begin();
        if (firstPart != relativeToRoot.end() && *firstPart != "..") {
            return relativePath;
        }
    }

    return (rootPath / asPath).string();
}

} // namespace AssetPaths
#pragma once
#include <string>

// Centralizes where the engine looks for game assets (models, audio,
// scripts, prefabs, scenes, shaders, config, saves) instead of scattering
// hardcoded relative path strings across engine and game code.
//
// Roots are configurable via config/engine.yaml under a "paths:" section,
// e.g.:
//   paths:
//     models: "custom/models"
// Any root not specified falls back to the project's actual default
// layout (flat folders at the project root — models/, audio/, etc.), so
// existing projects keep working unchanged until you choose to override.
namespace AssetPaths {

enum class Category {
    Models,
    Textures,
    Audio,
    Scripts,
    Prefabs,
    Scenes,
    Shaders,
    Config,
    Saves
};

// Loads paths.* from config/engine.yaml (via the global ConfigManager) on
// first call. Safe to call multiple times — later calls are no-ops.
void EnsureLoaded();

// Returns the configured root directory for a category, e.g. "models".
std::string Root(Category category);

// Joins the category's root with a relative path, e.g.
//   Resolve(Category::Models, "player/player.fbx") -> "models/player/player.fbx"
// If relativePath is already absolute, or already starts with the
// resolved root, it's returned unchanged — so existing JSON-authored
// paths (scene/chunk/prefab files that already say "models/x.obj") and
// full absolute paths (e.g. from the editor's asset browser) both keep
// working unchanged.
std::string Resolve(Category category, const std::string& relativePath);

} // namespace AssetPaths
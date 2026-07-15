#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include "../scene/Scene.h"
#include "../rendering/Camera.h"
#include "../rendering/GridRenderer.h"
#include "../physics/PhysicsWorld.h"
#include <entt/entt.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Texture;

class EditorUI {
public:
    void Initialize(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void Render();

    void QueueDroppedFiles(int count, const char** paths);
    void UpdatePerformanceStats(float deltaTime, int windowWidth, int windowHeight);
    void ToggleStatsOverlay();

    void DrawMenuBar();
    void DrawStatsOverlay();
    void DrawSceneHierarchy(Scene& scene);
    void DrawInspector(Scene& scene);
    void DrawAssetBrowser(Scene& scene);
    void DrawPhysicsPanel(Scene& scene, PhysicsWorld& physicsWorld);
    void DrawViewportSettings(Camera& camera, GridRenderer& gridRenderer);

    entt::entity SelectedEntity = entt::null;

    // Per-panel visibility, toggled from the menu bar.
    bool ShowSceneHierarchy = true;
    bool ShowInspector = true;
    bool ShowAssetBrowser = true;
    bool ShowPhysicsPanel = true;
    bool ShowViewportSettings = true;
    bool ShowStatsOverlay = false;

private:
    void EnsureAssetDirectories();
    void ImportPendingDroppedFiles();
    void DrawAssetEntry(Scene& scene, const std::filesystem::directory_entry& entry);
    void DrawDeleteAssetPopup();
    void BeginRenameAsset(const std::filesystem::path& assetPath);
    bool RenameAsset(const std::filesystem::path& assetPath, const std::string& newName);
    bool DeleteAsset(const std::filesystem::path& assetPath);
    bool AddAssetToScene(Scene& scene, const std::filesystem::path& assetPath);
    bool ApplyTextureToSelectedEntity(Scene& scene, const std::filesystem::path& assetPath);
    std::shared_ptr<Texture> GetOrLoadTexture(const std::filesystem::path& assetPath);
    bool CanCreateAssetFolderInCurrentPath() const;
    bool CanModifyAssetPath(const std::filesystem::path& assetPath) const;
    bool IsManagedAssetRoot(const std::filesystem::path& assetPath) const;
    bool IsInsideManagedAssetRoot(const std::filesystem::path& assetPath, bool allowRoot) const;
    bool IsSameOrChildPath(const std::filesystem::path& assetPath, const std::filesystem::path& rootPath) const;
    void EraseTextureCacheForPath(const std::filesystem::path& assetPath);
    std::filesystem::path GetImportDestinationFor(const std::filesystem::path& sourcePath) const;
    std::filesystem::path MakeUniqueDestination(const std::filesystem::path& destinationPath) const;
    std::string GetDisplayPath(const std::filesystem::path& path) const;
    std::filesystem::path NormalizePath(const std::filesystem::path& path) const;

    std::filesystem::path m_projectRoot;
    std::filesystem::path m_currentAssetPath;
    std::vector<std::filesystem::path> m_pendingDroppedFiles;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    float m_frameTimeMs = 0.0f;
    float m_fps = 0.0f;
    int m_windowWidth = 0;
    int m_windowHeight = 0;
    std::string m_gpuVendor;
    std::string m_gpuRenderer;
    std::string m_glVersion;
    std::string m_glslVersion;
    unsigned int m_hardwareConcurrency = 1u;
    char m_newFolderName[128] = {};
    char m_renameAssetName[128] = {};
    std::filesystem::path m_renamingAssetPath;
    std::filesystem::path m_deleteCandidatePath;
    bool m_isCreatingFolder = false;
    bool m_isRenamingAsset = false;
    bool m_shouldOpenDeletePopup = false;
    std::string m_assetStatusMessage;
};

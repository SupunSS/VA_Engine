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
#include <unordered_set>

class Texture;
class CharacterController; // used only by pointer here — full type comes from CharacterController.h in the .cpp
class ScriptEngine; // forward decl — only a reference is needed in the header

enum class GizmoOperation {
    Translate,
    Rotate,
    Scale
};

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
    bool DrawVehiclePanel(Scene& scene, PhysicsWorld& physicsWorld, class VehicleController* activeVehicle,
                       const glm::vec3& spawnPos, bool& outDespawnRequested);
    void DrawViewportSettings(Camera& camera, GridRenderer& gridRenderer);
    void DrawPlayerPanel(bool& playMode, CharacterController* controller);

    // Frustum + distance culling debug panel. freezeCullingFrustum is owned
    // by main.cpp (toggling it snapshots the active camera's frustum and
    // holds it while true, instead of tracking the live camera — lets you
    // fly the free-fly camera outside the frozen frustum to visually verify
    // culling). renderedCount/culledCount are read-only stats for display.
    void DrawCullingPanel(bool& freezeCullingFrustum, float& maxRenderDistance, int renderedCount, int culledCount);
    void DrawCullingDebugOverlay(float cameraYaw, float cameraPitch, float cameraDepthToVehicle,
                              float vehicleDistance, float vehicleRadius, bool vehicleWithinDistance,
                              bool vehicleInsideFrustum, int visibleWheelCount);

    // --- Gizmo / selection -------------------------------------------------
    void DrawGizmoToolbar();
    void DrawTransformGizmo(Scene& scene, PhysicsWorld& physicsWorld, const Camera& camera, float aspectRatio);
    void HandleViewportClick(Scene& scene, const Camera& camera, float aspectRatio,
                              double mouseX, double mouseY, int viewportWidth, int viewportHeight);
    void DeleteSelectedEntity(Scene& scene, PhysicsWorld& physicsWorld);
    bool IsGizmoActive() const;

    // --- Save / Load ---------------------------------------------------
    // Draws the Save/Load panel. Returns true if the user requested a save
    // or load action THIS frame — outSlotName is the chosen slot name,
    // outIsSaveAction is true for "Save", false for "Load". The caller
    // (main.cpp) is responsible for actually invoking SaveSystem and
    // applying the result — this panel only reports the intent.
    bool DrawSaveLoadPanel(std::string& outSlotName, bool& outIsSaveAction);

    // --- Script Editor ---------------------------------------------------
    // In-engine Lua editor: lists .lua files under game/scripts/, lets you
    // edit and save them, and immediately runs the saved script via the
    // real ScriptEngine. Also registers a HotReloadManager watch on first
    // save so external edits to the same file (e.g. from a text editor)
    // trigger a re-run too, without needing to return to this panel.
    void DrawScriptEditorPanel(ScriptEngine& scriptEngine);

    GizmoOperation CurrentGizmoOperation = GizmoOperation::Translate;

    entt::entity SelectedEntity = entt::null;

    // Per-panel visibility, toggled from the menu bar.
    bool ShowSceneHierarchy = true;
    bool ShowInspector = true;
    bool ShowAssetBrowser = true;
    bool ShowPhysicsPanel = true;
    bool ShowVehiclePanel = true;
    bool ShowViewportSettings = true;
    bool ShowStatsOverlay = false;
    bool ShowPlayerPanel = true;
    bool ShowGizmoToolbar = true;
    bool ShowCullingPanel = true;
    bool ShowSaveLoadPanel = true;
    bool ShowScriptEditorPanel = true;
private:
    void EnsureAssetDirectories();
    void RefreshScriptFileList();
    void LoadScriptIntoEditor(const std::filesystem::path& scriptPath);
    void SaveCurrentScript(ScriptEngine& scriptEngine);
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

    // --- Save / Load state -----------------------------------------------
    char m_saveSlotNameBuffer[128] = {};
    std::vector<std::string> m_cachedSaveSlots;
    bool m_saveSlotsLoaded = false; // lets us populate the list once instead of re-scanning disk every frame
    std::string m_deleteSaveCandidate;       
    bool m_shouldOpenDeleteSavePopup = false;

    // --- Script Editor state ----------------------------------------------
    std::vector<std::filesystem::path> m_scriptFiles;
    bool m_scriptFilesLoaded = false;
    std::filesystem::path m_selectedScriptPath;
    std::string m_scriptEditBuffer;
    bool m_scriptBufferDirty = false;
    std::string m_scriptStatusMessage;
    std::unordered_set<std::string> m_watchedScriptPaths; // avoid re-registering the same HotReload watch every save
};
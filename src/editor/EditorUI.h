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
#include "Command.h"
#include "../scene/Components.h"
#include "../rendering/TerrainChunk.h"

class Texture;
class CharacterController; // used only by pointer here — full type comes from CharacterController.h in the .cpp
class ScriptEngine; // forward decl — only a reference is needed in the header
class Animator;
class AnimationStateMachine;
class Model;
class Shader;
class TerrainSystem;

enum class GizmoOperation {
    Translate,
    Rotate,
    Scale
};

enum class Workspace {
    Full,
    Scripter,
    LevelDesigner,
    Animation
};

// One thing the Animator Editor can edit — either the player or whichever
// scene entity has an AnimatorComponent. main.cpp/EditorUI caller builds
// this list each frame; the editor itself owns no ECS access.
struct AnimatorEditTarget {
    std::string Key;            // stable per-target key, e.g. "player" or "entity_42" — used to persist graph layout/selection across frames
    std::string DisplayName;    // tab label, e.g. "Player" or "Entity 42"
    AnimationStateMachine* Machine = nullptr;
    Animator* AnimatorPtr = nullptr;
    std::shared_ptr<Model> SourceModel; // needed to LoadAnimation() when authoring new states/clips
};

// Per-target editor UI state (graph layout, selection, scratch buffers).
// Kept separate from AnimationStateMachine itself since none of this is
// runtime/gameplay data — it's purely how the editor is currently drawn.
struct AnimatorEditorTargetState {
    std::unordered_map<std::string, glm::vec2> NodePositions;
    glm::vec2 PanOffset{0.0f, 0.0f};
    std::string SelectedState;
    int SelectedTransitionIndex = -1;
    bool PreviewMode = false;
    float PreviewNormalizedTime = 0.0f;

    // Live preview viewport camera — orbit/zoom state persists per target
    // so switching tabs doesn't reset your framing each time.
    float PreviewOrbitYaw = -30.0f;
    float PreviewOrbitPitch = 20.0f;
    float PreviewDistance = -1.0f; // <0 = not yet auto-framed to the model's bounds

    char NewStateName[64] = {};
    char NewStateClipPath[256] = {};
    float NewStateBlendIn = 0.2f;
    glm::vec2 PendingNewStatePos{40.0f, 40.0f};

    char RenameBuffer[64] = {};
    char ClipPathBuffer[256] = {};
    std::string LastEditedState;

    char SaveAsPathBuffer[256] = {};

    char NewEventName[64] = {};
    float NewEventTime = 0.0f;

    std::string StatusMessage;
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
    void DrawInspector(Scene& scene, ScriptEngine& scriptEngine);
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

    // --- Undo/Redo -------------------------------------------------------
    // General-purpose, not scoped to any one tool (§9.1 of the Level Design
    // System doc) — gizmo transforms, prop placement, and prop deletion all
    // route through this same history.
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    const char* PeekUndoLabel() const;
    const char* PeekRedoLabel() const;

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

    // Read-only debug/preview panel: shows current state, blend progress,
    // live parameter values, and the defined states/transitions for the
    // player's animation state machine (if provided) and whichever entity
    // is currently selected in the Scene Hierarchy (if it has an
    // AnimatorComponent with a StateMachine). Either pointer pair may be
    // null/absent — the panel just shows "Not available" for that section.
    void DrawAnimationPanel(Scene& scene, AnimationStateMachine* playerStateMachine, Animator* playerAnimator);

    GizmoOperation CurrentGizmoOperation = GizmoOperation::Translate;

    void DrawTerrainPanel(TerrainSystem& terrain);

    // Owns terrain-sculpting mouse interaction: while TerrainEditMode is
    // on, left-click-drag raycasts against the terrain and applies the
    // active brush, pushing exactly one undo entry per stroke (mouse-down
    // to mouse-up), matching DrawTransformGizmo's drag-to-single-undo
    // pattern. No-ops entirely while TerrainEditMode is off, so normal
    // viewport selection/gizmo use is unaffected.
    void UpdateTerrainSculpting(TerrainSystem& terrain, const Camera& camera, float aspectRatio,
                                 GLFWwindow* window, float deltaTime);

    bool ShowTerrainPanel = true;
    bool TerrainEditMode = false;

    // --- Workspaces ---------------------------------------------------
    // Switches which panels are visible so each role only sees what's
    // relevant to their job. Does not touch docking/window positions —
    // just Show* visibility flags.
    void ApplyWorkspace(Workspace workspace);
    Workspace GetCurrentWorkspace() const { return m_currentWorkspace; }

    // Full authoring Animator Editor: visual state graph (drag/select
    // nodes, right-click to add a state, click an arrow to edit a
    // transition), bone hierarchy tree, and a scrubbable timeline with
    // events. Complements (does not replace) DrawAnimationPanel, which
    // stays a lightweight read-only debug view.
    void DrawAnimatorEditorPanel(const std::vector<AnimatorEditTarget>& targets);

    bool ShowAnimatorEditor = true;
    bool ShowAnimatorPreview = true;

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
    bool ShowAnimationPanel = true;
    bool ShowCullingDebugOverlay = true;
    
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

    // --- Terrain sculpting state -------------------------------------
    int m_terrainBrushType = 0; // 0=Raise, 1=Lower, 2=Smooth — matches TerrainSystem::BrushType ordering
    float m_terrainBrushRadius = 5.0f;
    float m_terrainBrushStrength = 3.0f;
    bool m_terrainStrokeActive = false;
    TerrainChunk* m_terrainStrokeChunk = nullptr;
    std::vector<float> m_terrainStrokeBeforeHeights;

    //undu redo system
    CommandHistory m_commandHistory;
    bool m_gizmoWasUsing = false;
    Transform m_gizmoDragStartTransform;

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

    Workspace m_currentWorkspace = Workspace::Full;

    void DrawAnimatorTargetEditor(const AnimatorEditTarget& target);
    void DrawStateGraphCanvas(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawStateInspector(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawTransitionInspector(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawBoneTree(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawAnimatorTimeline(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawAnimatorPreviewViewport(const AnimatorEditTarget& target, AnimatorEditorTargetState& state);
    void DrawAnimatorPreviewWindow(const std::vector<AnimatorEditTarget>& targets);

    std::unordered_map<std::string, AnimatorEditorTargetState> m_animatorEditorState;
    std::string m_previewSelectedTargetKey; // which target the separate preview window shows

    // Offscreen render target for the Animator Editor's live preview. One
    // shared FBO is sufficient — ImGui only ever executes the currently
    // active tab's contents each frame, so at most one target renders here
    // per frame regardless of how many tabs exist.
    std::unique_ptr<Shader> m_previewShader;
    unsigned int m_previewFBO = 0;
    unsigned int m_previewColorTexture = 0;
    unsigned int m_previewDepthRBO = 0;
    int m_previewFBOWidth = 0;
    int m_previewFBOHeight = 0;
};

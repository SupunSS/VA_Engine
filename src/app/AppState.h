#include <glad/glad.h>
#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "../physics/PhysicsWorld.h" // for JPH::BodyID
#include <memory>
#include "../scene/Scene.h"
#include "../scene/SpatialGrid.h"
#include "../scene/ChunkManager.h"
#include "../scene/TerrainSystem.h"
#include "../scripting/ScriptEngine.h"
#include "../physics/CharacterController.h"
#include "../rendering/Camera.h"
#include "../rendering/FollowCamera.h"
#include "../rendering/VehicleCamera.h"
#include "PostProcess.h"
#include "../rendering/Shader.h"
#include "../rendering/GridRenderer.h"
#include "../rendering/Skybox.h"
#include "../rendering/Frustum.h"
#include "../rendering/FrustumRenderer.h"
#include "../editor/EditorUI.h"
#include "../editor/HUD.h"
#include "../rendering/Model.h"
#include "../rendering/Animation.h"
#include "../rendering/Animator.h"
#include "../rendering/AnimationStateMachine.h"

struct GLFWwindow;

// Plain-data state shared by the engine's update/render/UI code, grouped by
// subsystem so each file only needs to touch the group it owns.

struct FrameState {
    float aspectRatio = 1.0f;
    int lastFramebufferWidth = 0;
    int lastFramebufferHeight = 0;
    float lastFrameTime = 0.0f;
    float profileLogTimer = 0.0f;
};

struct InputState {
    // Mouse look
    bool mouseLookEnabled = false;
    bool mouseLookNeedsReset = true;
    bool mouseLookDragged = false; // true once the mouse moved noticeably since press
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;

    // Previous-frame key states, used for press-edge detection
    bool altRWasPressed = false;
    bool spaceWasPressed = false;
    bool escWasPressed = false;
    bool deleteWasPressed = false;
    bool tWasPressed = false;
    bool fWasPressed = false;
    bool f5WasPressed = false;
    bool zWasPressed = false;
    bool yWasPressed = false;
    bool f1WasPressed = false;
};

struct PlayState {
    bool playMode = false;
    bool insideVehicle = false;
    entt::entity activeVehicleEntity = entt::null;

    // Blank-world mode (set when a project is created/opened)
    bool blankWorld = false;
    JPH::BodyID blankFloorBodyId; // default-constructed = invalid

    glm::vec3 playerSpawnPosition{0.0f, 1.0f, 0.0f};
    glm::vec3 initialCameraPosition{0.0f, 0.0f, 3.0f};
    float initialCameraYaw = -90.0f;
    float initialCameraPitch = 0.0f;
};

struct EditorFlags {
    bool showUI = true;
    bool showPlayControlsWindow = true;
};

struct CullingState {
    bool freezeFrustum = false;
    bool freezeFrustumWasEnabled = false;
    glm::mat4 frozenViewProjection{1.0f};
    glm::vec3 frozenCameraPosition{0.0f};
    float frozenYaw = -90.0f;
    float frozenPitch = 0.0f;
    float maxRenderDistance = 300.0f;
    float pedestrianSimulationDistance = 60.0f;
    int renderedCount = 0;
    int culledCount = 0;

    // Culling debug overlay values
    float debugVehicleDistance = 0.0f;
    float debugVehicleRadius = 0.0f;
    float debugVehicleDepth = 0.0f;
    bool debugVehicleWithinDistance = false;
    bool debugVehicleInsideFrustum = false;
    int debugVisibleWheelCount = 0;
};

// Core simulation objects. Declared in construction order, so they are destroyed
// in reverse: anything holding a reference to physicsWorld (characterController)
// is declared after it.
struct WorldState {
    std::unique_ptr<Scene> scene;
    std::unique_ptr<SpatialGrid> spatialGrid;
    std::unique_ptr<PhysicsWorld> physicsWorld;
    std::unique_ptr<CharacterController> characterController;
    std::unique_ptr<ChunkManager> chunkManager;
    std::unique_ptr<TerrainSystem> terrainSystem;
    std::unique_ptr<ScriptEngine> scriptEngine;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<FollowCamera> followCamera;
    std::unique_ptr<VehicleCamera> vehicleCamera;
};
// GPU-side objects. Created after the GL context exists (same places as before).
struct RenderState {
    std::unique_ptr<Shader> triangleShader;
    std::unique_ptr<Shader> triangleInstancedShader;
    std::unique_ptr<Shader> bloomThresholdShader;
    std::unique_ptr<Shader> bloomBlurShader;
    std::unique_ptr<Shader> bloomCompositeShader;
    std::unique_ptr<Shader> terrainShader;

    PostProcessTargets postProcess;
    GLuint fullscreenVAO = 0;

    std::unique_ptr<GridRenderer> gridRenderer;
    std::unique_ptr<Skybox> skybox;
    Frustum cullingFrustum;
    std::unique_ptr<FrustumRenderer> frustumRenderer;
};

struct EditorState {
    std::unique_ptr<EditorUI> ui;
    std::unique_ptr<HUD> hud;
};

// Everything that makes up the player: gameplay entity, visual child entity,
// model, animation clips, animator and state machine.
struct PlayerState {
    entt::entity entity = entt::null;       // gameplay entity (physics position, health, audio)
    entt::entity visualEntity = entt::null; // child entity carrying the mesh
    std::shared_ptr<Model> model;
    std::shared_ptr<Animation> idleAnim;
    std::shared_ptr<Animation> walkAnim;
    std::shared_ptr<Animation> runAnim;
    std::shared_ptr<Animator> animator;
    std::shared_ptr<AnimationStateMachine> stateMachine;
};
struct AppState {
    FrameState frame;
    InputState input;
    PlayState play;
    EditorFlags editor;
    CullingState culling;
    WorldState world;
    RenderState render;
    EditorState editorObjects;
    PlayerState player;
    GLFWwindow* window = nullptr;
};
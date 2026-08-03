#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "core/Log.h"
#include "core/Assert.h"
#include "core/memory/StackAllocator.h"
#include "core/memory/PoolAllocator.h"
#include "core/jobs/JobSystem.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "rendering/Shader.h"
#include "rendering/Camera.h"
#include "rendering/Texture.h"
#include "rendering/Model.h"
#include "rendering/Animator.h"
#include "scene/Scene.h"
#include "scene/Components.h"
#include "scene/SpatialGrid.h"
#include "scene/SceneLoader.h"
#include "scripting/ScriptEngine.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "scene/ChunkManager.h"
#include "editor/EditorUI.h"
#include <imgui.h>
#include "rendering/GridRenderer.h"
#include "physics/PhysicsWorld.h"
#include "rendering/Material.h"
#include "physics/CharacterController.h"
#include "physics/VehicleController.h"
#include "rendering/FollowCamera.h"
#include "rendering/VehicleCamera.h"
#include "rendering/Primitives.h"
#include "rendering/Skybox.h"
#include "rendering/Frustum.h"
#include "rendering/FrustumRenderer.h"
#include "scene/CityLayoutConfig.h"
#include "scene/PedestrianSystem.h"
#include "scene/PedestrianSpawnSystem.h"
#include "editor/HUD.h"
#include "audio/AudioEngine.h"
#include "scene/AudioSystem.h"
#include "scene/SaveSystem.h"
#include <engine/public/EnginePublic.h>
#include "engine/LuaBindings.h"
#include "core/AssetPaths.h"

namespace {
struct WindowUserData {
    Camera* camera;
    FollowCamera* followCamera;
    VehicleCamera* vehicleCamera;
    bool* playMode;
    bool* insideVehicle;
    float* aspectRatio;
    EditorUI* editorUI;
    bool* mouseLookEnabled;
    bool* mouseLookNeedsReset;
    double* lastCursorX;
    double* lastCursorY;
    bool* mouseLookDragged; // true once the mouse has moved noticeably since press
    Scene* scene;           // needed so the mouse callback can run viewport picking
};

void SetCursorMode(GLFWwindow* window, int cursorMode, bool centerCursor)
{
    glfwSetInputMode(window, GLFW_CURSOR, cursorMode);

    if (!centerCursor) {
        return;
    }

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwSetCursorPos(window, windowWidth * 0.5, windowHeight * 0.5);
}

glm::vec3 ComputeSunLightColor(const glm::vec3& sunDirection)
{
    const glm::vec3 kRayleighCoeff(5.5e-6f, 13.0e-6f, 22.4e-6f);
    const float kMieCoeff = 21e-6f * 1.1f;
    const float kPathReference = 8000.0f;

    float sinElevation = glm::max(sunDirection.y, 0.01f);
    float pathLength = kPathReference / sinElevation;

    glm::vec3 extinction = kRayleighCoeff + glm::vec3(kMieCoeff);
    glm::vec3 transmittance(
        std::exp(-extinction.x * pathLength),
        std::exp(-extinction.y * pathLength),
        std::exp(-extinction.z * pathLength));

    const glm::vec3 kBaseSunColor(1.0f, 0.96f, 0.9f);
    return kBaseSunColor * transmittance;
}

// --- Bloom post-process framebuffers ---------------------------------------
struct PostProcessTargets {
    GLuint hdrFBO = 0, hdrColorTexture = 0, hdrDepthRBO = 0;
    GLuint brightFBO = 0, brightTexture = 0;
    GLuint pingpongFBO[2] = { 0, 0 };
    GLuint pingpongTexture[2] = { 0, 0 };
    int fullWidth = 0, fullHeight = 0;
    int halfWidth = 0, halfHeight = 0;
};

void DestroyPostProcessTargets(PostProcessTargets& t)
{
    if (t.hdrColorTexture) glDeleteTextures(1, &t.hdrColorTexture);
    if (t.hdrDepthRBO) glDeleteRenderbuffers(1, &t.hdrDepthRBO);
    if (t.hdrFBO) glDeleteFramebuffers(1, &t.hdrFBO);
    if (t.brightTexture) glDeleteTextures(1, &t.brightTexture);
    if (t.brightFBO) glDeleteFramebuffers(1, &t.brightFBO);
    for (int i = 0; i < 2; ++i) {
        if (t.pingpongTexture[i]) glDeleteTextures(1, &t.pingpongTexture[i]);
        if (t.pingpongFBO[i]) glDeleteFramebuffers(1, &t.pingpongFBO[i]);
    }
    t = PostProcessTargets{};
}

GLuint CreateHalfResColorTarget(GLuint& fboOut, int width, int height)
{
    glGenFramebuffers(1, &fboOut);
    glBindFramebuffer(GL_FRAMEBUFFER, fboOut);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    ENGINE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "Bloom half-res framebuffer incomplete");

    return tex;
}

void CreatePostProcessTargets(PostProcessTargets& t, int width, int height)
{
    DestroyPostProcessTargets(t);
    t.fullWidth = std::max(1, width);
    t.fullHeight = std::max(1, height);
    t.halfWidth = std::max(1, width / 2);
    t.halfHeight = std::max(1, height / 2);

    glGenFramebuffers(1, &t.hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, t.hdrFBO);

    glGenTextures(1, &t.hdrColorTexture);
    glBindTexture(GL_TEXTURE_2D, t.hdrColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, t.fullWidth, t.fullHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.hdrColorTexture, 0);

    glGenRenderbuffers(1, &t.hdrDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, t.hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t.fullWidth, t.fullHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.hdrDepthRBO);

    ENGINE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "HDR scene framebuffer incomplete");

    t.brightTexture = CreateHalfResColorTarget(t.brightFBO, t.halfWidth, t.halfHeight);
    t.pingpongTexture[0] = CreateHalfResColorTarget(t.pingpongFBO[0], t.halfWidth, t.halfHeight);
    t.pingpongTexture[1] = CreateHalfResColorTarget(t.pingpongFBO[1], t.halfWidth, t.halfHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

entt::entity SpawnTestVehicle(Scene& scene, PhysicsWorld& physicsWorld, const glm::vec3& position) {
    constexpr float kHalfX = 0.9f;
    constexpr float kHalfY = 0.4f;
    constexpr float kHalfZ = 1.8f;
    constexpr float kSpawnHeightOffset = 0.90f;

    const glm::vec3 spawnPosition = position + glm::vec3(0.0f, kSpawnHeightOffset, 0.0f);

    auto vehicleEntity = scene.CreateEntity();
    scene.Registry.emplace<VehicleTag>(vehicleEntity);
    scene.Registry.emplace<VehicleOccupant>(vehicleEntity);

    auto& transform = scene.Registry.get<Transform>(vehicleEntity);
    transform.Position = spawnPosition;
    transform.Scale = glm::vec3(1.0f);

    auto chassisModel = Primitives::CreateVehicleBody(kHalfX, kHalfY, kHalfZ, 1.1f);
    auto chassisMaterial = std::make_shared<Material>();
    chassisMaterial->albedoTint = glm::vec3(0.08f, 0.40f, 0.80f);
    scene.Registry.emplace<MeshRenderer>(vehicleEntity, chassisModel, chassisMaterial);

    auto controller = std::make_shared<VehicleController>(physicsWorld, spawnPosition);
    auto& vehicleComp = scene.Registry.emplace<VehicleComponent>(vehicleEntity);
    vehicleComp.Controller = controller;

    AudioClipId engineClip = AudioEngine::Get().LoadClip(AssetPaths::Resolve(AssetPaths::Category::Audio, "sfx/vehicle_engine_loop.wav"));
    auto& engineAudio = scene.Registry.emplace<VehicleEngineAudio>(vehicleEntity);
    engineAudio.EngineLoopClip = engineClip;
    engineAudio.Handle = AudioEngine::Get().CreateSource3D(
        engineClip, position, true, true,
        engineAudio.MinVolume, 3.0f, 60.0f);

    constexpr float kWheelRadius = 0.35f;
    constexpr float kWheelWidth  = 0.25f;
    auto wheelModel = Primitives::CreateWheel(kWheelRadius, kWheelWidth, 16);

    auto tyreMaterial = std::make_shared<Material>();
    tyreMaterial->albedoTint = glm::vec3(0.12f, 0.12f, 0.12f);

    auto rimMaterial = std::make_shared<Material>();
    rimMaterial->albedoTint = glm::vec3(0.75f, 0.75f, 0.80f);

    for (int i = 0; i < 4; ++i) {
        auto wheelEntity = scene.CreateEntity();
        scene.Registry.emplace<MeshRenderer>(wheelEntity, wheelModel, tyreMaterial);

        glm::vec3 wheelPos;
        glm::quat wheelRot;
        controller->GetWheelTransform(i, wheelPos, wheelRot);

        auto& wt = scene.Registry.get<Transform>(wheelEntity);
        wt.Position = wheelPos;
        wt.Rotation = wheelRot;
        wt.Scale    = glm::vec3(1.0f);

        vehicleComp.WheelEntities[i] = wheelEntity;
    }

    Log::Info("Spawned low-poly vehicle entity with 4 wheels at ({}, {}, {}).",
              position.x, position.y, position.z);
    return vehicleEntity;
}

} // namespace

int main() {
    Log::Info("Engine starting up...");

    StackAllocator stack(1024);
    auto marker = stack.GetMarker();
    int* a = static_cast<int*>(stack.Allocate(sizeof(int)));
    *a = 42;
    Log::Info("StackAllocator test: value = {}", *a);
    stack.FreeToMarker(marker);

    PoolAllocator pool(sizeof(int), 10);
    int* b = static_cast<int*>(pool.Allocate());
    *b = 7;
    Log::Info("PoolAllocator test: value = {}", *b);
    pool.Free(b);

    {
        JobSystem jobs;
        std::atomic<int> counter{0};

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 8; ++i) {
            jobs.Submit([&counter, i] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                counter++;
            });
        }
        jobs.Wait();
        auto end = std::chrono::high_resolution_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
        Log::Info("JobSystem test: {} jobs completed in {:.1f}ms (counter = {})", 8, elapsedMs, counter.load());
    }

    ENGINE_ASSERT(glfwInit(), "GLFW failed to initialize");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VA Engine", nullptr, nullptr);
    ENGINE_ASSERT(window != nullptr, "Failed to create GLFW window");

    glfwMakeContextCurrent(window);
    ENGINE_ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to initialize GLAD");

    EditorUI editorUI;
    editorUI.Initialize(window);
    editorUI.ApplyWorkspace(Workspace::Full); // default on startup — devs switch via the Workspace menu
    HUD hud;
    if (!AudioEngine::Get().Initialize()) {
        Log::Info("AudioEngine failed to initialize — continuing without audio.");
    }
    Log::Info("OpenGL loaded: {}", (const char*)glGetString(GL_VERSION));

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    float aspectRatio = (float)width / (float)height;
    bool mouseLookEnabled = false;
    bool mouseLookNeedsReset = true;
    bool mouseLookDragged = false;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;

    Shader triangleShader(AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.vert"),
                       AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.frag"));
    Shader triangleInstancedShader(AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle_instanced.vert"),
                                AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.frag"));
    Shader bloomThresholdShader(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                             AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_threshold.frag"));
    Shader bloomBlurShader(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                        AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_blur.frag"));
    Shader bloomCompositeShader(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                             AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_composite.frag"));

    GLuint fullscreenVAO = 0;
    glGenVertexArrays(1, &fullscreenVAO);

    PostProcessTargets postProcess;
    CreatePostProcessTargets(postProcess, width, height);
    int lastKnownFramebufferWidth = width;
    int lastKnownFramebufferHeight = height;

    constexpr float kBloomThreshold = 1.6f;
    constexpr float kBloomKnee = 0.4f;
    constexpr float kBloomIntensity = 0.18f;
    constexpr int   kBloomBlurPasses = 5;
    constexpr float kExposure = 0.42f;

    auto defaultMaterial = std::make_shared<Material>();
    defaultMaterial->albedoTint = glm::vec3(1.0f, 1.0f, 1.0f);

    const glm::vec3 kDirLightDirection(-0.3f, -1.0f, -0.3f);

    Scene scene;
    SpatialGrid spatialGrid(50.0f);
    PhysicsWorld physicsWorld;

    auto playerEntity = scene.CreateEntity();
    scene.Registry.emplace<PlayerTag>(playerEntity);
    scene.Registry.emplace<Health>(playerEntity);
    scene.Registry.emplace<Ammo>(playerEntity);
    scene.Registry.get<Transform>(playerEntity).Position = glm::vec3(0.0f, 1.0f, 0.0f);

    scene.Registry.emplace<MovementState>(playerEntity);
    AudioClipId sfxFootstepWalk = AudioEngine::Get().LoadClip(AssetPaths::Resolve(AssetPaths::Category::Audio, "sfx/footstep_walk.wav"));
    AudioClipId sfxFootstepRun  = AudioEngine::Get().LoadClip(AssetPaths::Resolve(AssetPaths::Category::Audio, "sfx/footstep_run.wav"));
    auto& playerFootsteps = scene.Registry.emplace<FootstepAudio>(playerEntity);
    playerFootsteps.WalkStepClip = sfxFootstepWalk;
    playerFootsteps.RunStepClip  = sfxFootstepRun;

    auto playerVisualEntity = scene.CreateEntity();
    auto& playerVisualTransform = scene.Registry.get<Transform>(playerVisualEntity);
    playerVisualTransform.Parent = playerEntity;
    playerVisualTransform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    playerVisualTransform.Scale = glm::vec3(0.01f, 0.01f, 0.01f);

    auto playerModel = SceneLoader::GetOrLoadModel("player/player.fbx");
    scene.Registry.emplace<MeshRenderer>(playerVisualEntity, playerModel, nullptr);

    auto playerIdleAnim = playerModel->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Idle.fbx"));
    auto playerWalkAnim = playerModel->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Walking.fbx"));
    auto playerRunAnim  = playerModel->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Running.fbx"));

    Animator playerAnimator;
    playerAnimator.PlayAnimation(playerIdleAnim);

    enum class PlayerAnimState { Idle, Walk, Run };
    PlayerAnimState currentPlayerAnimState = PlayerAnimState::Idle;

    CharacterController characterController(physicsWorld, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 kPlayerSpawnPosition(0.0f, 1.0f, 0.0f);
    FollowCamera followCamera;
    VehicleCamera vehicleCamera;
    bool playMode = false;
    bool insideVehicle = false;
    entt::entity activeVehicleEntity = entt::null;

    ChunkManager chunkManager(50.0f, 6);

    ScriptEngine scriptEngine;
    scriptEngine.Initialize(&scene);

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

    const glm::vec3 kInitialCameraPosition = camera.Position;
    const float kInitialCameraYaw = camera.GetYaw();
    const float kInitialCameraPitch = camera.GetPitch();

    VAPublic::IEngine* engine = VAPublic::InitializeEngine("config/engine.yaml");
    VAPublic::ConnectEngineSystems(engine, &scene, &physicsWorld, &AudioEngine::Get(), &scriptEngine, &camera);
    
    // Bind the public VAPublic Engine API (Engine:Subscribe, Engine:Log,
    // Engine:SpawnEntity, etc.) into ScriptEngine's actual Lua state. Must
    // happen after ConnectEngineSystems() so `engine` is fully wired up,
    // and before any script that references the `Engine` global runs.
    RegisterLuaBindings(scriptEngine.GetLuaState(), engine);

    scriptEngine.RunScript(AssetPaths::Resolve(AssetPaths::Category::Scripts, "test.lua"));

    GridRenderer gridRenderer;
    Skybox skybox;

    Frustum cullingFrustum;
    FrustumRenderer frustumRenderer;
    bool freezeCullingFrustum = false;
    bool freezeCullingFrustumWasEnabled = false;
    glm::mat4 frozenViewProjection(1.0f);
    glm::vec3 frozenCameraPosition(0.0f);
    float frozenFrustumYaw = -90.0f;
    float frozenFrustumPitch = 0.0f;
    float maxRenderDistance = 300.0f;
    float pedestrianSimulationDistance = 60.0f;
    int renderedEntityCount = 0;
    int culledEntityCount = 0;

    PedestrianSpawnSystem::Config pedestrianSpawnConfig;
    pedestrianSpawnConfig.TargetPopulation = 40;
    pedestrianSpawnConfig.SpawnRadius = 40.0f;
    pedestrianSpawnConfig.DespawnRadius = 70.0f;
    pedestrianSpawnConfig.ChunkSize = 50.0f;

    {
        auto ambientEntity = scene.CreateEntity();
        scene.Registry.get<Transform>(ambientEntity).Position = kPlayerSpawnPosition;

        AudioClipId sfxAmbientCity = AudioEngine::Get().LoadClip(AssetPaths::Resolve(AssetPaths::Category::Audio, "ambient/city_loop.mp3"));
        auto& ambientSource = scene.Registry.emplace<AudioSource>(ambientEntity);
        ambientSource.Clip = sfxAmbientCity;
        ambientSource.Loop = true;
        ambientSource.Autoplay = true;
        ambientSource.Volume = 0.5f;
        ambientSource.MinDistance = 10.0f;
        ambientSource.MaxDistance = 150.0f;
        ambientSource.Handle = AudioEngine::Get().CreateSource3D(
            ambientSource.Clip, kPlayerSpawnPosition, true, true,
            ambientSource.Volume, ambientSource.MinDistance, ambientSource.MaxDistance);
    }

    float debugVehicleDistance = 0.0f;
    float debugVehicleRadius = 0.0f;
    float debugVehicleDepth = 0.0f;
    bool debugVehicleWithinDistance = false;
    bool debugVehicleInsideFrustum = false;
    int debugVisibleWheelCount = 0;

    float lastFrameTime = 0.0f;
    bool altRWasPressed = false;
    bool spaceWasPressed = false;
    bool escWasPressed = false;
    bool deleteWasPressed = false;
    bool fWasPressed = false;
    bool f5WasPressed = false;

    // UI Visibility Toggle Flag
    bool showUI = true;
    bool f1WasPressed = false;

    float profileLogTimer = 0.0f;
    auto profileStart = []() { return std::chrono::high_resolution_clock::now(); };
    auto profileMs = [](auto start) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - start).count();
    };

    auto DestroyAllVehicles = [&]() {
        auto existingVehicles = scene.Registry.view<VehicleTag, VehicleComponent>();
        std::vector<entt::entity> vehiclesToDestroy(existingVehicles.begin(), existingVehicles.end());
        for (auto oldVehicleEntity : vehiclesToDestroy) {
            if (scene.Registry.all_of<VehicleEngineAudio>(oldVehicleEntity)) {
                auto& engineAudio = scene.Registry.get<VehicleEngineAudio>(oldVehicleEntity);
                AudioEngine::Get().DestroySource(engineAudio.Handle);
            }

            auto& oldVehicleComp = scene.Registry.get<VehicleComponent>(oldVehicleEntity);
            for (auto wheelEnt : oldVehicleComp.WheelEntities) {
                if (wheelEnt != entt::null && scene.Registry.valid(wheelEnt)) {
                    scene.DestroyEntity(wheelEnt);
                }
            }
            scene.DestroyEntity(oldVehicleEntity);
        }
    };

    auto ResetToInitialState = [&]() {
        if (insideVehicle) {
            scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.01f);
            insideVehicle = false;
        }
        activeVehicleEntity = entt::null;

        DestroyAllVehicles();

        {
            std::vector<entt::entity> toDestroy;
            auto view = scene.Registry.view<RigidBody, PhysicsTestBody>();
            for (auto entity : view) {
                toDestroy.push_back(entity);
            }
            for (auto entity : toDestroy) {
                auto& rb = scene.Registry.get<RigidBody>(entity);
                if (!rb.BodyId.IsInvalid()) {
                    physicsWorld.DestroyBody(rb.BodyId);
                }
                scene.DestroyEntity(entity);
            }
        }

        characterController.SetPosition(kPlayerSpawnPosition);
        scene.Registry.get<Transform>(playerEntity).Position = kPlayerSpawnPosition;
        scene.Registry.get<Transform>(playerEntity).Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        currentPlayerAnimState = PlayerAnimState::Idle;
        playerAnimator.PlayAnimation(playerIdleAnim);

        camera.Position = kInitialCameraPosition;
        camera.SetYawPitch(kInitialCameraYaw, kInitialCameraPitch);

        editorUI.SelectedEntity = entt::null;
    };

    auto StopPlayModeKeepState = [&]() {
        playMode = false;
        SetCursorMode(window, GLFW_CURSOR_NORMAL, false);
        mouseLookEnabled = false;
        mouseLookNeedsReset = true;
    };

    auto StartPlayMode = [&]() {
        playMode = true;
        SetCursorMode(window, GLFW_CURSOR_DISABLED, true);
        mouseLookEnabled = false;
        mouseLookNeedsReset = true;

        characterController.SetPosition(kPlayerSpawnPosition);
        scene.Registry.get<Transform>(playerEntity).Position = kPlayerSpawnPosition;
        chunkManager.Update(kPlayerSpawnPosition, scene, physicsWorld, maxRenderDistance);
    };

    WindowUserData userData{
        &camera, &followCamera, &vehicleCamera, &playMode, &insideVehicle, &aspectRatio, &editorUI,
        &mouseLookEnabled, &mouseLookNeedsReset, &lastCursorX, &lastCursorY,
        &mouseLookDragged, &scene
    };
    glfwSetWindowUserPointer(window, &userData);

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent((float)xpos, (float)ypos);

        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));

        if (*userData->playMode) {
            if (*userData->mouseLookNeedsReset) {
                *userData->lastCursorX = xpos;
                *userData->lastCursorY = ypos;
                *userData->mouseLookNeedsReset = false;
            }
            float xOffset = (float)xpos - (float)*userData->lastCursorX;
            float yOffset = (float)*userData->lastCursorY - (float)ypos;
            *userData->lastCursorX = xpos;
            *userData->lastCursorY = ypos;
            if (*userData->insideVehicle) {
                userData->vehicleCamera->ProcessMouseMovement(xOffset, yOffset);
            } else {
                userData->followCamera->ProcessMouseMovement(xOffset, yOffset);
            }
            return;
        }

        if (io.WantCaptureMouse) return;

        Camera* cam = userData->camera;

        if (!*userData->mouseLookEnabled) {
            *userData->mouseLookNeedsReset = true;
            return;
        }

        if (*userData->mouseLookNeedsReset) {
            *userData->lastCursorX = xpos;
            *userData->lastCursorY = ypos;
            *userData->mouseLookNeedsReset = false;
        }

        float xOffset = (float)xpos - (float)*userData->lastCursorX;
        float yOffset = (float)*userData->lastCursorY - (float)ypos;
        *userData->lastCursorX = xpos;
        *userData->lastCursorY = ypos;

        if (std::abs(xOffset) > 1.0f || std::abs(yOffset) > 1.0f) {
            *userData->mouseLookDragged = true;
        }

        cam->ProcessMouseMovement(xOffset, yOffset);
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);

        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));

        if (*userData->playMode) return;

        if (io.WantCaptureMouse) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
                *userData->mouseLookEnabled = false;
                SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
            }
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                *userData->mouseLookEnabled = true;
                *userData->mouseLookNeedsReset = true;
                *userData->mouseLookDragged = false;
                SetCursorMode(win, GLFW_CURSOR_DISABLED, true);
            } else if (action == GLFW_RELEASE) {
                *userData->mouseLookEnabled = false;
                SetCursorMode(win, GLFW_CURSOR_NORMAL, false);

                if (!*userData->mouseLookDragged && !userData->editorUI->IsGizmoActive()) {
                    double mouseX = 0.0, mouseY = 0.0;
                    glfwGetCursorPos(win, &mouseX, &mouseY);
                    int fbWidth = 0, fbHeight = 0;
                    glfwGetFramebufferSize(win, &fbWidth, &fbHeight);
                    userData->editorUI->HandleViewportClick(
                        *userData->scene, *userData->camera, *userData->aspectRatio,
                        mouseX, mouseY, fbWidth, fbHeight);
                }
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent((float)xoffset, (float)yoffset);

        if (io.WantCaptureMouse) return;

        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));

        if (*userData->playMode) {
            if (*userData->insideVehicle) {
                userData->vehicleCamera->ProcessScroll((float)yoffset);
            } else {
                userData->followCamera->ProcessScroll((float)yoffset);
            }
            return;
        }

        bool ctrlHeld = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        if (ctrlHeld) {
            float increment = (float)yoffset * userData->camera->GetMoveSpeed() * 0.1f;
            userData->camera->AdjustMoveSpeed(increment);
        }
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int newWidth, int newHeight) {
        if (newHeight == 0) return;
        glViewport(0, 0, newWidth, newHeight);

        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
        *userData->aspectRatio = (float)newWidth / (float)newHeight;
    });

    glfwSetWindowFocusCallback(window, [](GLFWwindow* win, int focused) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
        if (userData == nullptr) {
            return;
        }

        if (!focused) {
            *userData->mouseLookEnabled = false;
            *userData->mouseLookNeedsReset = true;
        }

        if (!(*userData->playMode)) {
            SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
        }
    });

    glfwSetDropCallback(window, [](GLFWwindow* win, int count, const char** paths) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
        if (userData != nullptr && userData->editorUI != nullptr) {
            userData->editorUI->QueueDroppedFiles(count, paths);
        }
    });

    CityLayoutConfig::LoadFromFile(AssetPaths::Resolve(AssetPaths::Category::Config, "city_layout.json"));

    chunkManager.Update(camera.Position, scene, physicsWorld, maxRenderDistance);

    glfwShowWindow(window);
    glfwMaximizeWindow(window);
    glfwFocusWindow(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    Log::Info("Window created successfully");

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferHeight > 0) {
            aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
        }
        editorUI.UpdatePerformanceStats(deltaTime, framebufferWidth, framebufferHeight);

        if (framebufferWidth > 0 && framebufferHeight > 0 &&
            (framebufferWidth != lastKnownFramebufferWidth || framebufferHeight != lastKnownFramebufferHeight)) {
            CreatePostProcessTargets(postProcess, framebufferWidth, framebufferHeight);
            lastKnownFramebufferWidth = framebufferWidth;
            lastKnownFramebufferHeight = framebufferHeight;
        }

        // F1 Toggle for Editor UI
        const bool f1Held = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
        if (f1Held && !f1WasPressed) {
            showUI = !showUI;
        }
        f1WasPressed = f1Held;

        const bool altHeld = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        const bool rHeld = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (altHeld && rHeld && !altRWasPressed) {
            editorUI.ToggleStatsOverlay();
        }
        altRWasPressed = altHeld && rHeld;

        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

         // While any ImGui widget wants keyboard input (e.g. typing in the
        // Script Editor's text box), suppress WASD so it doesn't also drive
        // camera movement or player movement underneath the UI.
        const bool uiWantsKeyboard = ImGui::GetIO().WantCaptureKeyboard;
        if (uiWantsKeyboard) {
            w = false;
            s = false;
            a = false;
            d = false;
        }

        const bool f5Held = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
        if (f5Held && !f5WasPressed && !ImGui::GetIO().WantCaptureKeyboard) {
            if (playMode) {
                StopPlayModeKeepState();
            } else {
                StartPlayMode();
            }
        }
        f5WasPressed = f5Held;

        const bool escHeld = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool shiftHeldForEsc = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        if (escHeld && !escWasPressed && playMode) {
            StopPlayModeKeepState();

            if (shiftHeldForEsc) {
                ResetToInitialState();
            }
        }
        escWasPressed = escHeld;

        const bool deleteHeld = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
        if (!playMode && deleteHeld && !deleteWasPressed && !ImGui::GetIO().WantCaptureKeyboard) {
            editorUI.DeleteSelectedEntity(scene, physicsWorld);
        }
        deleteWasPressed = deleteHeld;

        if (!playMode) {
            camera.ProcessKeyboard(w, s, a, d, deltaTime);

            constexpr float kVerticalSpeedMultiplier = 2.0f;
            const bool spaceHeldEditor = !uiWantsKeyboard && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            const bool ctrlHeldEditor = !uiWantsKeyboard && (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
            if (spaceHeldEditor) {
                camera.Position += glm::vec3(0.0f, 1.0f, 0.0f) * camera.GetMoveSpeed() * kVerticalSpeedMultiplier * deltaTime;
            }
            if (ctrlHeldEditor) {
                camera.Position -= glm::vec3(0.0f, 1.0f, 0.0f) * camera.GetMoveSpeed() * kVerticalSpeedMultiplier * deltaTime;
            }

            if (!ImGui::GetIO().WantCaptureKeyboard) {
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                    editorUI.CurrentGizmoOperation = GizmoOperation::Translate;
                } else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                    editorUI.CurrentGizmoOperation = GizmoOperation::Rotate;
                }
            }
        }

        auto t_physicsStep = profileStart();
        physicsWorld.Step(deltaTime);
        double ms_physicsStep = profileMs(t_physicsStep);

        glm::vec3 viewerPosition = camera.Position;

        const bool fHeld = !uiWantsKeyboard && glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        if (playMode && fHeld && !fWasPressed) {
            if (!insideVehicle) {
                glm::vec3 playerPos = characterController.GetPosition();
                auto nearby = spatialGrid.QueryRadius(playerPos, 4.0f);
                entt::entity candidateVehicle = entt::null;
                for (auto ent : nearby) {
                    if (scene.Registry.valid(ent) && scene.Registry.all_of<VehicleTag, VehicleComponent>(ent)) {
                        candidateVehicle = ent;
                        break;
                    }
                }
                if (candidateVehicle != entt::null) {
                    insideVehicle = true;
                    activeVehicleEntity = candidateVehicle;
                    scene.Registry.get<VehicleOccupant>(activeVehicleEntity).DriverEntity = playerEntity;
                    scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.0f);
                    {
                        auto& vc = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                        if (vc.Controller) {
                            glm::vec3 snapPos; glm::quat snapRot;
                            vc.Controller->GetChassisTransform(snapPos, snapRot);
                            vehicleCamera.SetTarget(snapPos, snapRot, 0.0f, 0.016f);
                        }
                    }
                    Log::Info("Entered vehicle");
                }
            } else {
                if (activeVehicleEntity != entt::null && scene.Registry.valid(activeVehicleEntity)) {
                    auto& vehicleComp = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                    if (vehicleComp.Controller) {
                        glm::vec3 chassisPos;
                        glm::quat chassisRot;
                        vehicleComp.Controller->GetChassisTransform(chassisPos, chassisRot);
                        glm::vec3 exitPos = chassisPos - chassisRot * glm::vec3(2.0f, 0.0f, 0.0f);
                        characterController.SetPosition(exitPos);
                    }
                    scene.Registry.get<VehicleOccupant>(activeVehicleEntity).DriverEntity = entt::null;
                }
                insideVehicle = false;
                activeVehicleEntity = entt::null;
                scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.01f);
                Log::Info("Exited vehicle");
            }
        }
        fWasPressed = fHeld;

        std::string interactionPromptText;
        if (playMode) {
            if (insideVehicle) {
                interactionPromptText = "Press F to exit vehicle";
            } else {
                glm::vec3 playerPosForPrompt = characterController.GetPosition();
                auto nearbyForPrompt = spatialGrid.QueryRadius(playerPosForPrompt, 4.0f);
                for (auto ent : nearbyForPrompt) {
                    if (scene.Registry.valid(ent) && scene.Registry.all_of<VehicleTag, VehicleComponent>(ent)) {
                        interactionPromptText = "Press F to enter vehicle";
                        break;
                    }
                }
            }
        }

        if (playMode) {
            scene.Registry.get<MovementState>(playerEntity).IsMoving = false;
            scene.Registry.get<MovementState>(playerEntity).IsRunning = false;

            if (!insideVehicle) {
                glm::vec3 wishDir(0.0f);
                if (w) wishDir += followCamera.GetForwardXZ();
                if (s) wishDir -= followCamera.GetForwardXZ();
                if (d) wishDir += followCamera.GetRightXZ();
                if (a) wishDir -= followCamera.GetRightXZ();

                if (glm::length(wishDir) > 0.001f) {
                    glm::vec3 facingDir = glm::normalize(wishDir);
                    float targetYaw = std::atan2(facingDir.x, facingDir.z);
                    glm::quat targetRotation = glm::angleAxis(targetYaw, glm::vec3(0.0f, 1.0f, 0.0f));

                    glm::quat& currentRotation = scene.Registry.get<Transform>(playerEntity).Rotation;
                    constexpr float kTurnSpeed = 12.0f;
                    currentRotation = glm::slerp(currentRotation, targetRotation, glm::min(kTurnSpeed * deltaTime, 1.0f));
                }

                bool spaceHeld = !uiWantsKeyboard && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
                bool jumpEdge = spaceHeld && !spaceWasPressed;
                spaceWasPressed = spaceHeld;

                characterController.Sprinting = !uiWantsKeyboard && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                characterController.Update(deltaTime, wishDir, jumpEdge);

                {
                    auto& moveState = scene.Registry.get<MovementState>(playerEntity);
                    moveState.IsMoving = glm::length(wishDir) > 0.001f;
                    moveState.IsRunning = characterController.Sprinting;
                }

                PlayerAnimState desiredAnimState = PlayerAnimState::Idle;
                if (glm::length(wishDir) > 0.001f) {
                    desiredAnimState = characterController.Sprinting ? PlayerAnimState::Run : PlayerAnimState::Walk;
                }
                if (desiredAnimState != currentPlayerAnimState) {
                    currentPlayerAnimState = desiredAnimState;
                    switch (currentPlayerAnimState) {
                        case PlayerAnimState::Idle: playerAnimator.PlayAnimation(playerIdleAnim); break;
                        case PlayerAnimState::Walk: playerAnimator.PlayAnimation(playerWalkAnim); break;
                        case PlayerAnimState::Run:  playerAnimator.PlayAnimation(playerRunAnim);  break;
                    }
                }

                glm::vec3 playerPos = characterController.GetPosition();
                scene.Registry.get<Transform>(playerEntity).Position = playerPos;
                followCamera.SetTarget(playerPos);

                {
                    glm::vec3 rayOrigin = followCamera.GetTarget();
                    glm::vec3 toCamera = followCamera.Position - rayOrigin;
                    float desiredDistance = glm::length(toCamera);
                    if (desiredDistance > 0.001f) {
                        glm::vec3 rayDir = toCamera / desiredDistance;
                        glm::vec3 hitPoint;
                        if (physicsWorld.RaycastClosest(rayOrigin, rayDir, desiredDistance, hitPoint)) {
                            constexpr float kCameraCollisionBuffer = 0.2f;
                            followCamera.Position = hitPoint - rayDir * kCameraCollisionBuffer;
                        }
                    }
                }

                viewerPosition = playerPos;
            } else {
                float throttle = 0.0f;
                if (w) throttle += 1.0f;
                if (s) throttle -= 1.0f;

                float steer = 0.0f;
                if (d) steer += 1.0f;
                if (a) steer -= 1.0f;

                bool handbrake = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

                if (activeVehicleEntity != entt::null && scene.Registry.valid(activeVehicleEntity)) {
                    auto& vehicleComp = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                    if (vehicleComp.Controller) {
                        vehicleComp.Controller->Update(deltaTime, throttle, 0.0f, steer, handbrake);

                        glm::vec3 chassisPos;
                        glm::quat chassisRot;
                        vehicleComp.Controller->GetChassisTransform(chassisPos, chassisRot);
                        scene.Registry.get<Transform>(activeVehicleEntity).Position = chassisPos;
                        scene.Registry.get<Transform>(activeVehicleEntity).Rotation = chassisRot;

                        vehicleCamera.SetTarget(chassisPos, chassisRot, vehicleComp.Controller->GetSpeedKmh(), deltaTime);

                        glm::vec3 rayOrigin = chassisPos + glm::vec3(0.0f, 2.0f, 0.0f);
                        glm::vec3 toCamera = vehicleCamera.Position - rayOrigin;
                        float desiredDistance = glm::length(toCamera);
                        if (desiredDistance > 0.001f) {
                            glm::vec3 rayDir = toCamera / desiredDistance;
                            glm::vec3 hitPoint;
                            if (physicsWorld.RaycastClosest(rayOrigin, rayDir, desiredDistance, hitPoint)) {
                                constexpr float kCameraCollisionBuffer = 0.2f;
                                vehicleCamera.Position = hitPoint - rayDir * kCameraCollisionBuffer;
                            }
                        }

                        viewerPosition = chassisPos;
                    }
                }
            }
        }

        float frameTime = playMode ? deltaTime : 0.0f;
        playerAnimator.UpdateAnimation(frameTime);

        glBindFramebuffer(GL_FRAMEBUFFER, postProcess.hdrFBO);
        glViewport(0, 0, postProcess.fullWidth, postProcess.fullHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.35f, 0.35f, 0.38f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 activeView = playMode ? (insideVehicle ? vehicleCamera.GetViewMatrix() : followCamera.GetViewMatrix()) : camera.GetViewMatrix();
        const glm::mat4 activeProjection = playMode ? (insideVehicle ? vehicleCamera.GetProjectionMatrix(aspectRatio) : followCamera.GetProjectionMatrix(aspectRatio)) : camera.GetProjectionMatrix(aspectRatio);
        const glm::vec3 activeCameraPos = playMode ? (insideVehicle ? vehicleCamera.Position : followCamera.Position) : camera.Position;
        const glm::mat4 activeViewProjection = activeProjection * activeView;

        {
            const glm::mat4 invActiveViewForAudio = glm::inverse(activeView);
            const glm::vec3 listenerForward = glm::normalize(glm::vec3(invActiveViewForAudio * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            const glm::vec3 listenerUp = glm::normalize(glm::vec3(invActiveViewForAudio * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
            AudioSystem::UpdateListener(activeCameraPos, listenerForward, listenerUp);
        }
        AudioSystem::Update(scene, frameTime);

        static float engineTestTimer = 0.0f;
        engineTestTimer += deltaTime;
        if (engineTestTimer > 3.0f) {
            engineTestTimer = 0.0f;
            VAPublic::Vec3 posViaEngine = engine->GetPosition(static_cast<VAPublic::EntityId>(playerEntity));
            Log::Info("[EngineAPI smoke test] player position via IEngine: ({:.2f}, {:.2f}, {:.2f})",
                      posViaEngine.x, posViaEngine.y, posViaEngine.z);
        }

        if (freezeCullingFrustum) {
            if (!freezeCullingFrustumWasEnabled) {
                frozenCameraPosition = activeCameraPos;
                const glm::mat4 invActiveView = glm::inverse(activeView);
                const glm::vec3 activeForward = glm::normalize(glm::vec3(invActiveView * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                frozenFrustumPitch = glm::degrees(std::asin(glm::clamp(activeForward.y, -1.0f, 1.0f)));
                frozenFrustumYaw = glm::degrees(std::atan2(activeForward.z, activeForward.x));
            }

            if (!ImGui::GetIO().WantCaptureKeyboard) {
                constexpr float kFrustumRotateSpeed = 60.0f;
                if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  frozenFrustumYaw -= kFrustumRotateSpeed * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) frozenFrustumYaw += kFrustumRotateSpeed * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    frozenFrustumPitch += kFrustumRotateSpeed * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  frozenFrustumPitch -= kFrustumRotateSpeed * deltaTime;
                frozenFrustumPitch = glm::clamp(frozenFrustumPitch, -89.0f, 89.0f);
            }

            glm::vec3 frozenForward;
            frozenForward.x = std::cos(glm::radians(frozenFrustumYaw)) * std::cos(glm::radians(frozenFrustumPitch));
            frozenForward.y = std::sin(glm::radians(frozenFrustumPitch));
            frozenForward.z = std::sin(glm::radians(frozenFrustumYaw)) * std::cos(glm::radians(frozenFrustumPitch));
            frozenForward = glm::normalize(frozenForward);

            const glm::mat4 frozenView = glm::lookAt(frozenCameraPosition, frozenCameraPosition + frozenForward, glm::vec3(0.0f, 1.0f, 0.0f));
            frozenViewProjection = camera.GetProjectionMatrix(aspectRatio) * frozenView;
        } else {
            frozenViewProjection = activeViewProjection;
            frozenCameraPosition = activeCameraPos;
        }
        freezeCullingFrustumWasEnabled = freezeCullingFrustum;

        cullingFrustum = Frustum(frozenViewProjection);

        skybox.Render(activeView, activeProjection, activeCameraPos, -kDirLightDirection);

        if (!playMode) {
            gridRenderer.Render(camera, aspectRatio);
        }

        constexpr float kDirLightIntensity = 0.7f;
        const glm::vec3 dirLightColor = ComputeSunLightColor(-kDirLightDirection) * kDirLightIntensity;

        triangleShader.Bind();
        triangleShader.SetMat4("uView", activeView);
        triangleShader.SetMat4("uProjection", activeProjection);
        triangleShader.SetVec3("uViewPos", activeCameraPos);

        triangleShader.SetVec3("uDirLightDirection", kDirLightDirection);
        triangleShader.SetVec3("uDirLightColor", dirLightColor);
        triangleShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
        triangleShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));

        auto t_spatialGrid = profileStart();
        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
        }
        double ms_spatialGrid = profileMs(t_spatialGrid);

        auto t_chunkUpdate = profileStart();
        chunkManager.Update(viewerPosition, scene, physicsWorld, maxRenderDistance);
        double ms_chunkUpdate = profileMs(t_chunkUpdate);

        auto t_rigidBodySync = profileStart();
        auto rigidBodyView = scene.Registry.view<Transform, RigidBody>();
        for (auto entity : rigidBodyView) {
            auto& transform = rigidBodyView.get<Transform>(entity);
            auto& rigidBody = rigidBodyView.get<RigidBody>(entity);
            if (rigidBody.BodyId.IsInvalid()) {
                continue;
            }

            transform.Position = physicsWorld.GetBodyPosition(rigidBody.BodyId);
            transform.Rotation = physicsWorld.GetBodyRotation(rigidBody.BodyId);
        }

        auto vehicleView = scene.Registry.view<Transform, VehicleComponent>();
        for (auto entity : vehicleView) {
            auto& vehicleComp = vehicleView.get<VehicleComponent>(entity);
            if (!vehicleComp.Controller) continue;

            glm::vec3 chassisPos;
            glm::quat chassisRot;
            vehicleComp.Controller->GetChassisTransform(chassisPos, chassisRot);
            auto& transform = vehicleView.get<Transform>(entity);
            transform.Position = chassisPos;
            transform.Rotation = chassisRot;

            for (int i = 0; i < 4; ++i) {
                entt::entity wheelEntity = vehicleComp.WheelEntities[i];
                if (wheelEntity != entt::null && scene.Registry.valid(wheelEntity)) {
                    glm::vec3 wheelPos;
                    glm::quat wheelRot;
                    vehicleComp.Controller->GetWheelTransform(i, wheelPos, wheelRot);
                    auto& wheelTransform = scene.Registry.get<Transform>(wheelEntity);
                    wheelTransform.Position = wheelPos;
                    wheelTransform.Rotation = wheelRot;
                }
            }
        }
        double ms_rigidBodySync = profileMs(t_rigidBodySync);

        scriptEngine.CallUpdate(deltaTime);
        scriptEngine.CheckForReload(deltaTime);
        scriptEngine.CallEntityUpdates(deltaTime);

        PedestrianSystem::Update(scene, deltaTime, viewerPosition, pedestrianSimulationDistance);
        PedestrianSpawnSystem::Update(scene, viewerPosition, deltaTime, pedestrianSpawnConfig);

        static float queryTimer = 0.0f;
        queryTimer += deltaTime;
        if (queryTimer > 2.0f) {
            queryTimer = 0.0f;
            auto nearby = spatialGrid.QueryRadius(viewerPosition, 20.0f);
            Log::Info("Spatial query: {} entities within 20 units of viewer", nearby.size());
        }

        auto t_cullingAndDraw = profileStart();

        renderedEntityCount = 0;
        culledEntityCount = 0;

        struct InstanceGroupKey {
            Model* model;
            Material* material;
            bool operator==(const InstanceGroupKey& other) const {
                return model == other.model && material == other.material;
            }
        };
        struct InstanceGroupKeyHash {
            size_t operator()(const InstanceGroupKey& key) const {
                return std::hash<void*>()(key.model) ^ (std::hash<void*>()(key.material) << 1);
            }
        };

        static std::unordered_map<InstanceGroupKey, std::vector<glm::mat4>, InstanceGroupKeyHash> instanceGroups;
        instanceGroups.clear();

        std::unordered_set<entt::entity> debugVehicleEntities;
        if (activeVehicleEntity != entt::null && scene.Registry.valid(activeVehicleEntity)) {
            debugVehicleEntities.insert(activeVehicleEntity);
            if (scene.Registry.all_of<VehicleComponent>(activeVehicleEntity)) {
                auto& vc = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                for (auto wheelEnt : vc.WheelEntities) {
                    if (wheelEnt != entt::null) {
                        debugVehicleEntities.insert(wheelEnt);
                    }
                }
            }
        }

        debugVisibleWheelCount = 0;

        const glm::mat4 invActiveViewForDebug = glm::inverse(activeView);
        const glm::vec3 viewForwardForDebug = glm::normalize(glm::vec3(invActiveViewForDebug * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

        const auto nearbyEntities = spatialGrid.QueryRadius(frozenCameraPosition, maxRenderDistance);

        for (entt::entity entity : nearbyEntities) {
            if (!scene.Registry.valid(entity) || !scene.Registry.all_of<Transform, MeshRenderer>(entity)) {
                continue;
            }
            auto& transform = scene.Registry.get<Transform>(entity);
            auto& renderer = scene.Registry.get<MeshRenderer>(entity);

            if (!renderer.ModelRef) {
                continue;
            }

            glm::mat4 worldMatrix = scene.GetWorldMatrix(entity);

            const glm::vec3 localMin = renderer.ModelRef->GetBoundsMin();
            const glm::vec3 localMax = renderer.ModelRef->GetBoundsMax();
            const glm::vec3 localCenter = (localMin + localMax) * 0.5f;
            const glm::vec3 localHalfExtent = (localMax - localMin) * 0.5f;

            const glm::vec3 worldCenter = glm::vec3(worldMatrix * glm::vec4(localCenter, 1.0f));
            const float scaleX = glm::length(glm::vec3(worldMatrix[0]));
            const float scaleY = glm::length(glm::vec3(worldMatrix[1]));
            const float scaleZ = glm::length(glm::vec3(worldMatrix[2]));
            const float maxScale = glm::max(scaleX, glm::max(scaleY, scaleZ));
            const float worldRadius = glm::length(localHalfExtent) * maxScale;

            const float distanceToCamera = glm::length(worldCenter - frozenCameraPosition);
            const bool withinDistance = distanceToCamera <= (maxRenderDistance + worldRadius);
            constexpr float kFrustumCullingMargin = 0.5f;
            const bool insideFrustum = cullingFrustum.IntersectsSphere(worldCenter, worldRadius + kFrustumCullingMargin);

            if (scene.Registry.all_of<PedestrianTag>(entity)) {
                const glm::vec3 toViewer = worldCenter - frozenCameraPosition;
                const float pedDistSq = toViewer.x * toViewer.x + toViewer.y * toViewer.y + toViewer.z * toViewer.z;
                if (pedDistSq > pedestrianSimulationDistance * pedestrianSimulationDistance) {
                    ++culledEntityCount;
                    continue;
                }
            }

            if (debugVehicleEntities.contains(entity)) {
                const glm::vec3 toObject = worldCenter - frozenCameraPosition;
                const float depthAlongView = glm::dot(toObject, viewForwardForDebug);

                if (entity == activeVehicleEntity) {
                    debugVehicleDistance = distanceToCamera;
                    debugVehicleRadius = worldRadius;
                    debugVehicleDepth = depthAlongView;
                    debugVehicleWithinDistance = withinDistance;
                    debugVehicleInsideFrustum = insideFrustum;
                }

                if (!withinDistance || !insideFrustum) {
                    Log::Info("[CullDebug] entity {} CULLED — dist={:.2f} radius={:.2f} depth={:.2f} withinDist={} insideFrustum={}",
                        static_cast<uint32_t>(entity), distanceToCamera, worldRadius, depthAlongView, withinDistance, insideFrustum);
                } else {
                    ++debugVisibleWheelCount;
                }
            }

            if (!withinDistance || !insideFrustum) {
                ++culledEntityCount;
                continue;
            }
            ++renderedEntityCount;

            if (entity == playerVisualEntity) {
                triangleShader.SetMat4("uModel", worldMatrix);
                triangleShader.SetMat4Array("uBoneMatrices", playerAnimator.GetFinalBoneMatrices());
                renderer.ModelRef->Draw(triangleShader, renderer.MaterialRef ? renderer.MaterialRef.get() : nullptr);
                continue;
            }

            if (scene.Registry.all_of<AnimatorComponent>(entity)) {
                auto& animComp = scene.Registry.get<AnimatorComponent>(entity);
                if (animComp.AnimatorPtr) {
                    triangleShader.SetMat4("uModel", worldMatrix);
                    triangleShader.SetMat4Array("uBoneMatrices", animComp.AnimatorPtr->GetFinalBoneMatrices());
                    renderer.ModelRef->Draw(triangleShader, renderer.MaterialRef ? renderer.MaterialRef.get() : nullptr);
                }
                continue;
            }

            InstanceGroupKey key{ renderer.ModelRef.get(), renderer.MaterialRef.get() };
            instanceGroups[key].push_back(worldMatrix);
        }

        bool instancedShaderBoundThisFrame = false;
        for (auto& [key, matrices] : instanceGroups) {
            if (matrices.size() == 1) {
                triangleShader.Bind();
                triangleShader.SetMat4("uModel", matrices[0]);
                key.model->Draw(triangleShader, key.material);
                continue;
            }

            if (!instancedShaderBoundThisFrame) {
                triangleInstancedShader.Bind();
                triangleInstancedShader.SetMat4("uView", activeView);
                triangleInstancedShader.SetMat4("uProjection", activeProjection);
                triangleInstancedShader.SetVec3("uViewPos", activeCameraPos);
                triangleInstancedShader.SetVec3("uDirLightDirection", kDirLightDirection);
                triangleInstancedShader.SetVec3("uDirLightColor", dirLightColor);
                triangleInstancedShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
                triangleInstancedShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));
                instancedShaderBoundThisFrame = true;
            }
            key.model->DrawInstanced(triangleInstancedShader, key.material, matrices);
        }

        if (!playMode && freezeCullingFrustum) {
            frustumRenderer.Render(frozenViewProjection, activeViewProjection);
        }

        double ms_cullingAndDraw = profileMs(t_cullingAndDraw);

        auto t_bloom = profileStart();

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(fullscreenVAO);

        glBindFramebuffer(GL_FRAMEBUFFER, postProcess.brightFBO);
        glViewport(0, 0, postProcess.halfWidth, postProcess.halfHeight);
        bloomThresholdShader.Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, postProcess.hdrColorTexture);
        bloomThresholdShader.SetInt("uSceneColor", 0);
        bloomThresholdShader.SetFloat("uThreshold", kBloomThreshold);
        bloomThresholdShader.SetFloat("uKnee", kBloomKnee);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        bool horizontal = true;
        GLuint sourceTexture = postProcess.brightTexture;
        bloomBlurShader.Bind();
        bloomBlurShader.SetInt("uSourceTexture", 0);
        bloomBlurShader.SetVec2("uTexelSize", glm::vec2(1.0f / postProcess.halfWidth, 1.0f / postProcess.halfHeight));

        for (int i = 0; i < kBloomBlurPasses * 2; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, postProcess.pingpongFBO[horizontal ? 0 : 1]);
            bloomBlurShader.SetInt("uHorizontal", horizontal ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sourceTexture);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            sourceTexture = postProcess.pingpongTexture[horizontal ? 0 : 1];
            horizontal = !horizontal;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        bloomCompositeShader.Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, postProcess.hdrColorTexture);
        bloomCompositeShader.SetInt("uSceneColor", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        bloomCompositeShader.SetInt("uBloomTexture", 1);
        bloomCompositeShader.SetFloat("uBloomIntensity", kBloomIntensity);
        bloomCompositeShader.SetFloat("uExposure", kExposure);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);

        double ms_bloom = profileMs(t_bloom);

        profileLogTimer += deltaTime;
        if (profileLogTimer > 1.0f) {
            profileLogTimer = 0.0f;
            Log::Info("Profile ms — physics: {:.2f} chunk: {:.2f} grid: {:.2f} rbSync: {:.2f} cull+draw: {:.2f} bloom: {:.2f} total: {:.2f}",
                ms_physicsStep, ms_chunkUpdate, ms_spatialGrid, ms_rigidBodySync, ms_cullingAndDraw, ms_bloom,
                ms_physicsStep + ms_chunkUpdate + ms_spatialGrid + ms_rigidBodySync + ms_cullingAndDraw + ms_bloom);
        }

        // --- Editor UI & HUD Rendering -------------------------------------
        editorUI.BeginFrame();

        // 1. Gameplay HUD (Renders during playMode regardless of showUI)
        if (playMode) {
            hud.SetViewportSize(framebufferWidth, framebufferHeight);

            std::vector<HUD::MinimapBlip> minimapBlips;
            {
                auto mmVehicleView = scene.Registry.view<Transform, VehicleComponent>();
                for (auto ent : mmVehicleView) {
                    auto& t = mmVehicleView.get<Transform>(ent);
                    minimapBlips.push_back({ glm::vec2(t.Position.x, t.Position.z), HUD::MinimapBlip::BlipType::Vehicle });
                }
                auto mmPedView = scene.Registry.view<Transform, PedestrianTag>();
                for (auto ent : mmPedView) {
                    auto& t = mmPedView.get<Transform>(ent);
                    minimapBlips.push_back({ glm::vec2(t.Position.x, t.Position.z), HUD::MinimapBlip::BlipType::Pedestrian });
                }
            }

            const glm::vec3& hudPlayerPos = scene.Registry.get<Transform>(playerEntity).Position;
            hud.DrawMinimap(glm::vec2(hudPlayerPos.x, hudPlayerPos.z), camera.GetYaw(), minimapBlips);

            const auto& playerHealth = scene.Registry.get<Health>(playerEntity);
            hud.DrawHealthBar(playerHealth.Current, playerHealth.Max);

            const auto& playerAmmo = scene.Registry.get<Ammo>(playerEntity);
            hud.DrawAmmoCounter(playerAmmo.Current, playerAmmo.Reserve);

            float hudSpeedKmh = 0.0f, hudRpm = 0.0f;
            int hudGear = 0;
            if (insideVehicle && activeVehicleEntity != entt::null && scene.Registry.valid(activeVehicleEntity)) {
                auto& vc = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                if (vc.Controller) {
                    hudSpeedKmh = vc.Controller->GetSpeedKmh();
                    hudRpm = vc.Controller->GetRPM();
                    hudGear = vc.Controller->GetTransmissionGear();
                }
            }
            hud.DrawSpeedometer(insideVehicle, hudSpeedKmh, hudRpm, hudGear);

            hud.DrawInteractionPrompt(interactionPromptText);
        }

        // 2. Editor Windows & Tools (Toggled on/off using F1)
        if (showUI) {
            {
                ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("Play Controls");
                ImGui::Text("Status: %s", playMode ? "Playing" : "Stopped");
                ImGui::Separator();
                static float masterVolume = 1.0f;
                if (ImGui::SliderFloat("Master Volume", &masterVolume, 0.0f, 1.0f)) {
                    AudioEngine::Get().SetMasterVolume(masterVolume);
                }
                ImGui::Separator();
                ImGui::TextUnformatted("F1        : Toggle Editor UI");
                ImGui::TextUnformatted("F5        : Play / Stop (keeps world state)");
                ImGui::TextUnformatted("Esc       : Stop (keeps world state)");
                ImGui::TextUnformatted("Shift+Esc : Stop + Full Reset");
                ImGui::Separator();
                if (ImGui::Button("Full Reset", ImVec2(120.0f, 0.0f))) {
                    if (playMode) {
                        StopPlayModeKeepState();
                    }
                    ResetToInitialState();
                }
                ImGui::End();
            }

            editorUI.DrawMenuBar();
            editorUI.DrawSceneHierarchy(scene);
            editorUI.DrawInspector(scene, scriptEngine);
            editorUI.DrawAssetBrowser(scene);
            editorUI.DrawPhysicsPanel(scene, physicsWorld);
            editorUI.DrawScriptEditorPanel(scriptEngine);

            VehicleController* activeVehiclePtr = nullptr;
            if (activeVehicleEntity != entt::null && scene.Registry.valid(activeVehicleEntity) && scene.Registry.all_of<VehicleComponent>(activeVehicleEntity)) {
                activeVehiclePtr = scene.Registry.get<VehicleComponent>(activeVehicleEntity).Controller.get();
            } else {
                auto vView = scene.Registry.view<VehicleComponent>();
                if (!vView.empty()) {
                    activeVehiclePtr = vView.get<VehicleComponent>(*vView.begin()).Controller.get();
                }
            }

            bool despawnVehicleRequested = false;
            const bool spawnVehicleRequested = editorUI.DrawVehiclePanel(scene, physicsWorld, activeVehiclePtr, viewerPosition, despawnVehicleRequested);

            if (spawnVehicleRequested) {
                if (insideVehicle) {
                    scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.01f);
                    insideVehicle = false;
                }
                activeVehicleEntity = entt::null;

                DestroyAllVehicles();

                const glm::vec3 playerPos = scene.Registry.get<Transform>(playerEntity).Position;
                glm::vec3 forwardFlat = camera.GetFront();
                forwardFlat.y = 0.0f;
                if (glm::length(forwardFlat) < 0.001f) forwardFlat = glm::vec3(0.0f, 0.0f, -1.0f);
                forwardFlat = glm::normalize(forwardFlat);

                constexpr float kSpawnDistance = 5.0f;
                constexpr float kSpawnHeight = 3.0f;
                const glm::vec3 spawnPos = playerPos + forwardFlat * kSpawnDistance + glm::vec3(0.0f, kSpawnHeight, 0.0f);

                activeVehicleEntity = SpawnTestVehicle(scene, physicsWorld, spawnPos);
            }

            editorUI.DrawGizmoToolbar();
            editorUI.DrawTransformGizmo(scene, physicsWorld, camera, aspectRatio);

            bool playModeBeforePanel = playMode;
            editorUI.DrawPlayerPanel(playMode, &characterController);
            if (playMode != playModeBeforePanel) {
                if (playMode) {
                    StartPlayMode();
                } else {
                    StopPlayModeKeepState();
                }
            }

            editorUI.DrawViewportSettings(camera, gridRenderer);

            {
                std::string requestedSlotName;
                bool isSaveAction = false;
                if (editorUI.DrawSaveLoadPanel(requestedSlotName, isSaveAction)) {
                    if (isSaveAction) {
                        SaveGameData data;

                        auto& savePlayerTransform = scene.Registry.get<Transform>(playerEntity);
                        data.Player.PlayerTransform.Position = savePlayerTransform.Position;
                        data.Player.PlayerTransform.Rotation = savePlayerTransform.Rotation;

                        auto& saveHealth = scene.Registry.get<Health>(playerEntity);
                        data.Player.Health = saveHealth.Current;
                        data.Player.MaxHealth = saveHealth.Max;

                        auto& saveAmmo = scene.Registry.get<Ammo>(playerEntity);
                        data.Player.AmmoCurrent = saveAmmo.Current;
                        data.Player.AmmoReserve = saveAmmo.Reserve;

                        data.Player.InsideVehicle = insideVehicle;
                        data.Player.ActiveVehicleIndex = -1;

                        int vehicleIndex = 0;
                        auto saveVehicleView = scene.Registry.view<Transform, VehicleTag, VehicleComponent>();
                        for (auto vEntity : saveVehicleView) {
                            auto& vTransform = saveVehicleView.get<Transform>(vEntity);
                            SavedVehicleState vs;
                            vs.ChassisTransform.Position = vTransform.Position;
                            vs.ChassisTransform.Rotation = vTransform.Rotation;
                            data.Vehicles.push_back(vs);

                            if (vEntity == activeVehicleEntity) {
                                data.Player.ActiveVehicleIndex = vehicleIndex;
                            }
                            ++vehicleIndex;
                        }

                        SaveSystem::WriteSaveFile(requestedSlotName, data);
                        Log::Info("Saved game to slot '{}'.", requestedSlotName);
                    } else {
                        SaveGameData data;
                        if (SaveSystem::ReadSaveFile(requestedSlotName, data)) {
                            if (insideVehicle) {
                                scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.01f);
                                insideVehicle = false;
                            }
                            activeVehicleEntity = entt::null;
                            DestroyAllVehicles();

                            characterController.SetPosition(data.Player.PlayerTransform.Position);
                            auto& loadPlayerTransform = scene.Registry.get<Transform>(playerEntity);
                            loadPlayerTransform.Position = data.Player.PlayerTransform.Position;
                            loadPlayerTransform.Rotation = data.Player.PlayerTransform.Rotation;

                            auto& loadHealth = scene.Registry.get<Health>(playerEntity);
                            loadHealth.Current = data.Player.Health;
                            loadHealth.Max = data.Player.MaxHealth;

                            auto& loadAmmo = scene.Registry.get<Ammo>(playerEntity);
                            loadAmmo.Current = data.Player.AmmoCurrent;
                            loadAmmo.Reserve = data.Player.AmmoReserve;

                            // Re-stream every loaded chunk so building/entity deltas apply
                            chunkManager.ForceReloadAll(scene, physicsWorld);
                            chunkManager.Update(data.Player.PlayerTransform.Position, scene, physicsWorld, maxRenderDistance);

                            // Recreate vehicles at their saved transforms
                            std::vector<entt::entity> recreatedVehicles;
                            for (const auto& vehicleData : data.Vehicles) {
                                entt::entity newVehicle = SpawnTestVehicle(scene, physicsWorld, vehicleData.ChassisTransform.Position);
                                auto& vc = scene.Registry.get<VehicleComponent>(newVehicle);
                                if (vc.Controller) {
                                    vc.Controller->SetChassisTransform(vehicleData.ChassisTransform.Position, vehicleData.ChassisTransform.Rotation);
                                }
                                recreatedVehicles.push_back(newVehicle);
                            }

                            // Restore vehicle occupancy if the player was driving when they saved
                            if (data.Player.InsideVehicle && data.Player.ActiveVehicleIndex >= 0 &&
                                data.Player.ActiveVehicleIndex < static_cast<int>(recreatedVehicles.size())) {
                                activeVehicleEntity = recreatedVehicles[data.Player.ActiveVehicleIndex];
                                insideVehicle = true;
                                scene.Registry.get<VehicleOccupant>(activeVehicleEntity).DriverEntity = playerEntity;
                                scene.Registry.get<Transform>(playerVisualEntity).Scale = glm::vec3(0.0f);

                                glm::vec3 snapPos; glm::quat snapRot;
                                auto& vc = scene.Registry.get<VehicleComponent>(activeVehicleEntity);
                                if (vc.Controller) {
                                    vc.Controller->GetChassisTransform(snapPos, snapRot);
                                    vehicleCamera.SetTarget(snapPos, snapRot, 0.0f, 0.016f);
                                }
                            }

                            Log::Info("Loaded save slot '{}'.", requestedSlotName);
                        } else {
                            Log::Warn("Failed to load save slot '{}'.", requestedSlotName);
                        }
                    }
                }
            }
            editorUI.DrawCullingPanel(freezeCullingFrustum, maxRenderDistance, renderedEntityCount, culledEntityCount);
            editorUI.DrawCullingDebugOverlay(camera.GetYaw(), camera.GetPitch(), debugVehicleDepth,
                                      debugVehicleDistance, debugVehicleRadius,
                                      debugVehicleWithinDistance, debugVehicleInsideFrustum,
                                      debugVisibleWheelCount);
            editorUI.DrawStatsOverlay();
        }

        editorUI.Render();

        glfwSwapBuffers(window);
    }

    DestroyPostProcessTargets(postProcess);
    glDeleteVertexArrays(1, &fullscreenVAO);

    AudioEngine::Get().Shutdown();

    glfwDestroyWindow(window);
    editorUI.Shutdown();
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}
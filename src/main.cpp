#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
#include "rendering/AnimationStateMachine.h"
#include "rendering/AnimationStateMachineLoader.h"
#include "audio/FootstepClipLoader.h"
#include "scene/TerrainSystem.h"
#include "app/PostProcess.h"
#include "app/WindowUtils.h"
#include "app/Lighting.h"
#include "app/VehicleSpawner.h"
#include "app/AppState.h"
#include "app/Window.h"



int main() {
    Log::Info("Engine starting up...");

        AppState app;

    // Aliases: let the rest of main() keep compiling unchanged while code moves
    // into other files. Each alias is deleted when the code using it moves out.
    float& aspectRatio = app.frame.aspectRatio;
    int& lastKnownFramebufferWidth = app.frame.lastFramebufferWidth;
    int& lastKnownFramebufferHeight = app.frame.lastFramebufferHeight;
    float& lastFrameTime = app.frame.lastFrameTime;
    float& profileLogTimer = app.frame.profileLogTimer;

    bool& mouseLookEnabled = app.input.mouseLookEnabled;
    bool& mouseLookNeedsReset = app.input.mouseLookNeedsReset;
    bool& mouseLookDragged = app.input.mouseLookDragged;
    double& lastCursorX = app.input.lastCursorX;
    double& lastCursorY = app.input.lastCursorY;
    bool& altRWasPressed = app.input.altRWasPressed;
    bool& spaceWasPressed = app.input.spaceWasPressed;
    bool& escWasPressed = app.input.escWasPressed;
    bool& deleteWasPressed = app.input.deleteWasPressed;
    bool& tWasPressed = app.input.tWasPressed;
    bool& fWasPressed = app.input.fWasPressed;
    bool& f5WasPressed = app.input.f5WasPressed;
    bool& zWasPressed = app.input.zWasPressed;
    bool& yWasPressed = app.input.yWasPressed;
    bool& f1WasPressed = app.input.f1WasPressed;

    bool& playMode = app.play.playMode;
    bool& insideVehicle = app.play.insideVehicle;
    entt::entity& activeVehicleEntity = app.play.activeVehicleEntity;
    bool& blankWorld = app.play.blankWorld;
    JPH::BodyID& blankFloorBodyId = app.play.blankFloorBodyId;

    bool& showUI = app.editor.showUI;
    bool& showPlayControlsWindow = app.editor.showPlayControlsWindow;

    bool& freezeCullingFrustum = app.culling.freezeFrustum;
    bool& freezeCullingFrustumWasEnabled = app.culling.freezeFrustumWasEnabled;
    glm::mat4& frozenViewProjection = app.culling.frozenViewProjection;
    glm::vec3& frozenCameraPosition = app.culling.frozenCameraPosition;
    float& frozenFrustumYaw = app.culling.frozenYaw;
    float& frozenFrustumPitch = app.culling.frozenPitch;
    float& maxRenderDistance = app.culling.maxRenderDistance;
    float& pedestrianSimulationDistance = app.culling.pedestrianSimulationDistance;
    int& renderedEntityCount = app.culling.renderedCount;
    int& culledEntityCount = app.culling.culledCount;
    float& debugVehicleDistance = app.culling.debugVehicleDistance;
    float& debugVehicleRadius = app.culling.debugVehicleRadius;
    float& debugVehicleDepth = app.culling.debugVehicleDepth;
    bool& debugVehicleWithinDistance = app.culling.debugVehicleWithinDistance;
    bool& debugVehicleInsideFrustum = app.culling.debugVehicleInsideFrustum;
    int& debugVisibleWheelCount = app.culling.debugVisibleWheelCount;

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

    GLFWwindow* window = CreateMainWindow();

    app.editorObjects.ui = std::make_unique<EditorUI>();
    EditorUI& editorUI = *app.editorObjects.ui;
    editorUI.Initialize(window);
    editorUI.ApplyWorkspace(Workspace::Full); // default on startup — devs switch via the Workspace menu
    app.editorObjects.hud = std::make_unique<HUD>();
    HUD& hud = *app.editorObjects.hud;
    if (!AudioEngine::Get().Initialize()) {
        Log::Info("AudioEngine failed to initialize — continuing without audio.");
    }
    Log::Info("OpenGL loaded: {}", (const char*)glGetString(GL_VERSION));

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    aspectRatio = (float)width / (float)height;

    app.render.triangleShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.vert"),
                       AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.frag"));
    app.render.triangleInstancedShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle_instanced.vert"),
                                AssetPaths::Resolve(AssetPaths::Category::Shaders, "triangle.frag"));
    app.render.bloomThresholdShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                             AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_threshold.frag"));
    app.render.bloomBlurShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                        AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_blur.frag"));
    app.render.bloomCompositeShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "sky.vert"),
                             AssetPaths::Resolve(AssetPaths::Category::Shaders, "bloom_composite.frag"));
    Shader& triangleShader = *app.render.triangleShader;
    Shader& triangleInstancedShader = *app.render.triangleInstancedShader;
    Shader& bloomThresholdShader = *app.render.bloomThresholdShader;
    Shader& bloomBlurShader = *app.render.bloomBlurShader;
    Shader& bloomCompositeShader = *app.render.bloomCompositeShader;

    GLuint& fullscreenVAO = app.render.fullscreenVAO;
    glGenVertexArrays(1, &fullscreenVAO);

    PostProcessTargets& postProcess = app.render.postProcess;
    CreatePostProcessTargets(postProcess, width, height);
    lastKnownFramebufferWidth = width;
    lastKnownFramebufferHeight = height;

    constexpr float kBloomThreshold = 1.6f;
    constexpr float kBloomKnee = 0.4f;
    constexpr float kBloomIntensity = 0.18f;
    constexpr int   kBloomBlurPasses = 5;
    constexpr float kExposure = 0.42f;

    auto defaultMaterial = std::make_shared<Material>();
    defaultMaterial->albedoTint = glm::vec3(1.0f, 1.0f, 1.0f);

    const glm::vec3 kDirLightDirection(-0.3f, -1.0f, -0.3f);

    app.world.scene = std::make_unique<Scene>();
    app.world.spatialGrid = std::make_unique<SpatialGrid>(50.0f);
    app.world.physicsWorld = std::make_unique<PhysicsWorld>();
    Scene& scene = *app.world.scene;
    SpatialGrid& spatialGrid = *app.world.spatialGrid;
    PhysicsWorld& physicsWorld = *app.world.physicsWorld;

    auto playerEntity = scene.CreateEntity();
    scene.Registry.emplace<PlayerTag>(playerEntity);
    scene.Registry.emplace<Health>(playerEntity);
    scene.Registry.emplace<Ammo>(playerEntity);
    scene.Registry.get<Transform>(playerEntity).Position = glm::vec3(0.0f, 1.0f, 0.0f);

    scene.Registry.emplace<MovementState>(playerEntity);
    auto& playerFootsteps = scene.Registry.emplace<FootstepAudio>(playerEntity);
    playerFootsteps.WalkStepClips = FootstepClipLoader::LoadNumberedSequence("sfx/Steps_floor-", 1, 21, ".wav", 3);
    // No separate run clips — reuses the same pool for both walk and run
    // (PickRandomFootstepClip falls back to WalkStepClips when RunStepClips
    // is empty). Add a dedicated RunStepClips pool later if you get
    // sprint-specific footstep audio.

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
    
    
    auto playerAnimator = std::make_shared<Animator>();
    playerAnimator->PlayAnimation(playerIdleAnim);

    auto playerAnimStateMachinePtr = AnimationStateMachineLoader::LoadFromFile("player.json", *playerModel);
    ENGINE_ASSERT(playerAnimStateMachinePtr != nullptr, "Failed to load player animation state machine");
    AnimationStateMachine& playerAnimStateMachine = *playerAnimStateMachinePtr;
    playerAnimStateMachine.SetInitialState("Idle");

    // Attach the player's animator to the ECS so systems that iterate
    // AnimatorComponent (e.g. AudioSystem's event-driven footstep path)
    // pick up the player the same way they already do pedestrians.
    auto& playerAnimComp = scene.Registry.emplace<AnimatorComponent>(playerEntity);
    playerAnimComp.AnimatorPtr = playerAnimator;
    playerAnimComp.StateMachine = playerAnimStateMachinePtr;
    playerAnimComp.SourceModel = playerModel;

    app.world.characterController = std::make_unique<CharacterController>(physicsWorld, glm::vec3(0.0f, 1.0f, 0.0f));
    CharacterController& characterController = *app.world.characterController;
    const glm::vec3 kPlayerSpawnPosition(0.0f, 1.0f, 0.0f);
    app.world.followCamera = std::make_unique<FollowCamera>();
    app.world.vehicleCamera = std::make_unique<VehicleCamera>();
    FollowCamera& followCamera = *app.world.followCamera;
    VehicleCamera& vehicleCamera = *app.world.vehicleCamera;

    app.world.chunkManager = std::make_unique<ChunkManager>(50.0f, 6);
    app.world.terrainSystem = std::make_unique<TerrainSystem>(50.0f, 33, 4); // chunkSize, resolution, loadRadius — chunkSize matches ChunkManager's
    ChunkManager& chunkManager = *app.world.chunkManager;
    TerrainSystem& terrainSystem = *app.world.terrainSystem;
    app.render.terrainShader = std::make_unique<Shader>(AssetPaths::Resolve(AssetPaths::Category::Shaders, "terrain.vert"),
                      AssetPaths::Resolve(AssetPaths::Category::Shaders, "terrain.frag"));
    Shader& terrainShader = *app.render.terrainShader;

    app.world.scriptEngine = std::make_unique<ScriptEngine>();
    ScriptEngine& scriptEngine = *app.world.scriptEngine;
    scriptEngine.Initialize(&scene);

    app.world.camera = std::make_unique<Camera>(glm::vec3(0.0f, 0.0f, 3.0f));
    Camera& camera = *app.world.camera;

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

    app.render.gridRenderer = std::make_unique<GridRenderer>();
    app.render.skybox = std::make_unique<Skybox>();
    app.render.frustumRenderer = std::make_unique<FrustumRenderer>();
    GridRenderer& gridRenderer = *app.render.gridRenderer;
    Skybox& skybox = *app.render.skybox;
    Frustum& cullingFrustum = app.render.cullingFrustum;
    FrustumRenderer& frustumRenderer = *app.render.frustumRenderer;

    PedestrianSpawnSystem::Config pedestrianSpawnConfig;
    pedestrianSpawnConfig.TargetPopulation = 40;
    pedestrianSpawnConfig.SpawnRadius = 40.0f;
    pedestrianSpawnConfig.DespawnRadius = 70.0f;
    pedestrianSpawnConfig.ChunkSize = 50.0f;
    pedestrianSpawnConfig.Archetypes = {
        { "pedestrian_casual.json", 1.4f },
        { "pedestrian_brisk.json", 2.6f }
    };

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

        playerAnimStateMachine.SetInitialState("Idle");
        playerAnimator->PlayAnimation(playerIdleAnim);

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

        auto ResetWorldForProject = [&]() {
        if (playMode) {
            StopPlayModeKeepState();
        }
        ResetToInitialState(); // vehicles, test bodies, player, camera, selection

        // Remove the chunk-streamed city and stop it streaming back in
        chunkManager.ForceReloadAll(scene, physicsWorld);
        chunkManager.SetStreamingEnabled(false);

        // Remove pedestrians
        {
            std::vector<entt::entity> pedestrians;
            for (auto e : scene.Registry.view<PedestrianTag>()) {
                pedestrians.push_back(e);
            }
            for (auto e : pedestrians) {
                scene.DestroyEntity(e);
            }
        }

        // Fresh flat terrain
        terrainSystem.Clear();
        terrainSystem.Update(camera.Position);

        // One big static floor whose top sits at y=0, matching the flat terrain.
        // Physics only: terrain already provides the visual, and a mesh here
        // would need models/cube.obj, which a fresh project doesn't have.
        if (blankFloorBodyId.IsInvalid()) {
            const glm::vec3 floorHalfExtents(1000.0f, 0.5f, 1000.0f);
            const glm::vec3 floorCenter(0.0f, -0.5f, 0.0f);
            blankFloorBodyId = physicsWorld.CreateBoxBody(floorCenter, floorHalfExtents, /*isStatic=*/true);
        }

        blankWorld = true;
    };

    InstallWindowCallbacks(window, app);

    CityLayoutConfig::LoadFromFile(AssetPaths::Resolve(AssetPaths::Category::Config, "city_layout.json"));

    chunkManager.Update(camera.Position, scene, physicsWorld, maxRenderDistance);
    terrainSystem.Update(camera.Position);

    ShowMainWindow(window);

    Log::Info("Window created successfully");

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Clamp against huge deltaTime spikes (minimized/dragged window, a
        // debugger breakpoint, an asset-load hitch, or a slow first frame).
        // Unclamped, this can tunnel dynamic physics bodies through static
        // geometry in a single Step() and feeds equally large jumps into
        // animation/gameplay code. A full fixed-timestep accumulator would be
        // the more thorough fix, but CharacterController::Update() and
        // VehicleController::Update() are both called once per frame with
        // this same deltaTime, and I don't have their .cpp bodies to confirm
        // how they apply input to their Jolt bodies — decoupling physics
        // substeps from those calls without that visibility risks a subtler
        // desync. Clamping is the safe, contained fix.
        constexpr float kMaxDeltaTime = 0.1f; // 10 FPS floor before slow-motion instead of instability
        deltaTime = std::clamp(deltaTime, 0.0f, kMaxDeltaTime);

        glfwPollEvents();

        if (editorUI.ConsumeProjectChanged()) {
            ResetWorldForProject();
        }


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

        const bool ctrlHeldForUndo = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                      glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        const bool zHeld = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
        const bool yHeld = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS;
        if (!playMode && ctrlHeldForUndo && zHeld && !zWasPressed && !ImGui::GetIO().WantCaptureKeyboard) {
            editorUI.Undo();
        }
        zWasPressed = zHeld;
        if (!playMode && ctrlHeldForUndo && yHeld && !yWasPressed && !ImGui::GetIO().WantCaptureKeyboard) {
            editorUI.Redo();
        }
        yWasPressed = yHeld;

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

        if (!playMode) {
            editorUI.UpdateTerrainSculpting(terrainSystem, camera, aspectRatio, window, deltaTime);
        }
        
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

                playerAnimStateMachine.SetBool("IsMoving", glm::length(wishDir) > 0.001f);
                playerAnimStateMachine.SetBool("IsRunning", characterController.Sprinting);
                playerAnimStateMachine.Update(*playerAnimator);

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

                        // chassisPos/chassisRot are only needed locally here
                        // (camera target + the collision raycast just below).
                        // This used to also write scene.Registry's Transform
                        // for this entity directly — redundant, since the
                        // authoritative vehicleView sync loop later this same
                        // frame calls GetChassisTransform() again and writes
                        // the same values. Same result either way (idempotent,
                        // not conflicting), just wasted work every frame.
                        glm::vec3 chassisPos;
                        glm::quat chassisRot;
                        vehicleComp.Controller->GetChassisTransform(chassisPos, chassisRot);

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
        playerAnimator->UpdateAnimation(frameTime);

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
            // Was always camera.GetProjectionMatrix(...) — the editor camera's
            // projection specifically, regardless of whether followCamera or
            // vehicleCamera is actually active this frame. activeProjection is
            // already computed above, correctly per-mode — use that instead.
            frozenViewProjection = activeProjection * frozenView;
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

        terrainSystem.Render(terrainShader, activeView, activeProjection, activeCameraPos, kDirLightDirection, dirLightColor);

        triangleShader.Bind();
        triangleShader.SetMat4("uView", activeView);
        triangleShader.SetMat4("uProjection", activeProjection);
        triangleShader.SetVec3("uViewPos", activeCameraPos);

        triangleShader.SetVec3("uDirLightDirection", kDirLightDirection);
        triangleShader.SetVec3("uDirLightColor", dirLightColor);
        triangleShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
        triangleShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));

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
                if (!blankWorld) {
            PedestrianSpawnSystem::Update(scene, viewerPosition, deltaTime, pedestrianSpawnConfig);
        }

        // Spatial grid rebuilt AFTER every system above that can move an
        // entity this frame (physics sync, vehicle/wheel sync, pedestrian AI,
        // chunk streaming). Previously this ran BEFORE those systems, so the
        // grid — and therefore the culling query below, plus the vehicle
        // enter/exit check earlier this frame — worked off one-step-stale
        // positions for every rigid body, vehicle, and pedestrian.
        auto t_spatialGrid = profileStart();
        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
        }
        double ms_spatialGrid = profileMs(t_spatialGrid);

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

            if (blankWorld && !playMode && entity == playerVisualEntity) {
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
                triangleShader.SetMat4Array("uBoneMatrices", playerAnimator->GetFinalBoneMatrices());
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
            if (showPlayControlsWindow) {
                ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("Play Controls", &showPlayControlsWindow);
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
            editorUI.DrawProjectMenu();
            editorUI.DrawSceneHierarchy(scene);
            editorUI.DrawInspector(scene, scriptEngine);
            editorUI.DrawAssetBrowser(scene);
            editorUI.DrawPhysicsPanel(scene, physicsWorld);
            editorUI.DrawScriptEditorPanel(scriptEngine);
            editorUI.DrawAnimationPanel(scene, playerAnimStateMachinePtr.get(), playerAnimator.get());
            editorUI.DrawTerrainPanel(terrainSystem);
            
// --- Animator Editor (full authoring) --------------------------
            std::vector<AnimatorEditTarget> animatorEditTargets;
            animatorEditTargets.push_back({
                "player", "Player",
                playerAnimStateMachinePtr.get(),
                playerAnimator.get(),
                playerModel
            });

            if (editorUI.SelectedEntity != entt::null && scene.Registry.valid(editorUI.SelectedEntity) &&
                scene.Registry.all_of<AnimatorComponent>(editorUI.SelectedEntity)) {
                auto& animComp = scene.Registry.get<AnimatorComponent>(editorUI.SelectedEntity);
                if (animComp.StateMachine && animComp.AnimatorPtr) {
                    std::string key = "entity_" + std::to_string(static_cast<uint32_t>(editorUI.SelectedEntity));
                    animatorEditTargets.push_back({
                        key, "Selected Entity",
                        animComp.StateMachine.get(),
                        animComp.AnimatorPtr.get(),
                        animComp.SourceModel
                    });
                }
            }

            editorUI.DrawAnimatorEditorPanel(animatorEditTargets);

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

    // Must run BEFORE the window/GL context is destroyed — it tears down
    // ImGui's OpenGL backend (deletes GL objects, needs a live context) and
    // its GLFW backend (ImGui_ImplGlfw_Shutdown references the GLFWwindow*
    // directly). Previously this ran AFTER glfwDestroyWindow(), operating on
    // an already-destroyed context/window.
    editorUI.Shutdown();

    AudioEngine::Get().Shutdown();

    // VAPublic::InitializeEngine() has a matching VAPublic::ShutdownEngine()
    // (src/engine/Engine.cpp) that was never being called — this unwatches
    // HotReloadManager, unloads PrefabManager, and shuts down the event
    // system. Previously `engine` just leaked on exit with none of that
    // cleanup running. Call before GLFW teardown since none of what it does
    // depends on GLFW/GL being alive, so ordering here isn't load-bearing —
    // just keeping it grouped with the rest of the shutdown sequence.
    VAPublic::ShutdownEngine();

    glfwDestroyWindow(window);
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}
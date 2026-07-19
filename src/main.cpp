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
#include "rendering/FollowCamera.h"
#include "rendering/Skybox.h"

namespace {
struct WindowUserData {
    Camera* camera;
    FollowCamera* followCamera;
    bool* playMode;
    float* aspectRatio;
    EditorUI* editorUI;
    bool* mouseLookEnabled;
    bool* mouseLookNeedsReset;
    double* lastCursorX;
    double* lastCursorY;
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
// hdrFBO: full-resolution scene render target (RGBA16F, linear HDR values,
// no tonemapping applied by sky.frag/triangle.frag anymore) + a depth
// renderbuffer so normal depth-tested scene rendering still works.
// brightFBO / pingpongFBO: half-resolution targets used only for the bloom
// extraction + blur — bloom is inherently soft, so full-res blur would cost
// far more than it's worth visually.
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

    // Full-res HDR scene target.
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

    // Half-res bloom extraction + ping-pong blur targets.
    t.brightTexture = CreateHalfResColorTarget(t.brightFBO, t.halfWidth, t.halfHeight);
    t.pingpongTexture[0] = CreateHalfResColorTarget(t.pingpongFBO[0], t.halfWidth, t.halfHeight);
    t.pingpongTexture[1] = CreateHalfResColorTarget(t.pingpongFBO[1], t.halfWidth, t.halfHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    Log::Info("OpenGL loaded: {}", (const char*)glGetString(GL_VERSION));

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    float aspectRatio = (float)width / (float)height;
    bool mouseLookEnabled = false;
    bool mouseLookNeedsReset = true;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;

    Shader triangleShader("shaders/triangle.vert", "shaders/triangle.frag");

    // Bloom pipeline shaders — all three reuse sky.vert as their vertex
    // stage, since it's already the engine's no-VBO fullscreen triangle.
    Shader bloomThresholdShader("shaders/sky.vert", "shaders/bloom_threshold.frag");
    Shader bloomBlurShader("shaders/sky.vert", "shaders/bloom_blur.frag");
    Shader bloomCompositeShader("shaders/sky.vert", "shaders/bloom_composite.frag");

    // Empty VAO required by core-profile GL to issue a draw call even
    // though the fullscreen triangle vertex shader has no vertex attributes
    // (positions are computed purely from gl_VertexID).
    GLuint fullscreenVAO = 0;
    glGenVertexArrays(1, &fullscreenVAO);

    PostProcessTargets postProcess;
    CreatePostProcessTargets(postProcess, width, height);
    int lastKnownFramebufferWidth = width;
    int lastKnownFramebufferHeight = height;

    // Bloom tuning constants — start here if the effect looks too weak/strong.
    constexpr float kBloomThreshold = 1.0f;   // luminance above this starts blooming
    constexpr float kBloomKnee = 0.5f;        // soft transition width around the threshold
    constexpr float kBloomIntensity = 0.3f;   // how much blurred glow gets added back
    constexpr int   kBloomBlurPasses = 5;     // ping-pong iterations (10 total blur draws)
    constexpr float kExposure = 0.6f;         // overall scene exposure before tonemapping

    auto defaultMaterial = std::make_shared<Material>();
    defaultMaterial->albedoTint = glm::vec3(1.0f, 1.0f, 1.0f);

    const glm::vec3 kDirLightDirection(-0.3f, -1.0f, -0.3f);

    Scene scene;
    SpatialGrid spatialGrid(50.0f);
    PhysicsWorld physicsWorld;

    auto playerEntity = scene.CreateEntity();
    scene.Registry.emplace<PlayerTag>(playerEntity);
    scene.Registry.get<Transform>(playerEntity).Position = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Player visual: rigged mesh + skeletal animation --------------------
    auto playerVisualEntity = scene.CreateEntity();
    auto& playerVisualTransform = scene.Registry.get<Transform>(playerVisualEntity);
    playerVisualTransform.Parent = playerEntity;
    playerVisualTransform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    // Mixamo exports in centimeters — a ~170cm-tall humanoid needs scaling
    // down by 0.01 to match this engine's meter-scale units.
    playerVisualTransform.Scale = glm::vec3(0.01f, 0.01f, 0.01f);

    auto playerModel = SceneLoader::GetOrLoadModel("models/player/player.fbx");
    scene.Registry.emplace<MeshRenderer>(playerVisualEntity, playerModel, nullptr);

    auto playerIdleAnim = playerModel->LoadAnimation("models/player/Idle.fbx");
    auto playerWalkAnim = playerModel->LoadAnimation("models/player/Walking.fbx");
    auto playerRunAnim  = playerModel->LoadAnimation("models/player/Running.fbx");

    Animator playerAnimator;
    playerAnimator.PlayAnimation(playerIdleAnim);

    enum class PlayerAnimState { Idle, Walk, Run };
    PlayerAnimState currentPlayerAnimState = PlayerAnimState::Idle;

    CharacterController characterController(physicsWorld, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 kPlayerSpawnPosition(0.0f, 1.0f, 0.0f);
    FollowCamera followCamera;
    bool playMode = false;

    ChunkManager chunkManager(50.0f, 1);

    ScriptEngine scriptEngine;
    scriptEngine.Initialize(&scene);
    scriptEngine.RunScript("scripts/test.lua");

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

    GridRenderer gridRenderer;
    Skybox skybox;
    float lastFrameTime = 0.0f;
    bool altRWasPressed = false;
    bool spaceWasPressed = false;
    bool escWasPressed = false;

    WindowUserData userData{
        &camera, &followCamera, &playMode, &aspectRatio, &editorUI,
        &mouseLookEnabled, &mouseLookNeedsReset, &lastCursorX, &lastCursorY
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
            userData->followCamera->ProcessMouseMovement(xOffset, yOffset);
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
                SetCursorMode(win, GLFW_CURSOR_DISABLED, true);
            } else if (action == GLFW_RELEASE) {
                *userData->mouseLookEnabled = false;
                SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent((float)xoffset, (float)yoffset);

        if (io.WantCaptureMouse) return;

        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));

        if (*userData->playMode) {
            userData->followCamera->ProcessScroll((float)yoffset);
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
        // Post-process framebuffer resizing is handled in the main loop
        // (comparing against lastKnownFramebufferWidth/Height) rather than
        // here, since recreating GL objects from inside a GLFW callback —
        // which can fire mid-frame — is asking for trouble.
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

    glfwShowWindow(window);
    glfwMaximizeWindow(window);
    glfwFocusWindow(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Windows sometimes fails to compute non-client frame extents for a
    // window that was hidden at maximize time. SWP_FRAMECHANGED forces the
    // OS to recalculate and redraw the title bar/border explicitly, rather
    // than relying on maximize alone to trigger it. Must run after
    // glfwMaximizeWindow so it recalculates against the final window state.
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

        // Recreate the HDR/bloom framebuffers if the window was resized —
        // done here rather than in the resize callback itself (see comment
        // on glfwSetFramebufferSizeCallback above).
        if (framebufferWidth > 0 && framebufferHeight > 0 &&
            (framebufferWidth != lastKnownFramebufferWidth || framebufferHeight != lastKnownFramebufferHeight)) {
            CreatePostProcessTargets(postProcess, framebufferWidth, framebufferHeight);
            lastKnownFramebufferWidth = framebufferWidth;
            lastKnownFramebufferHeight = framebufferHeight;
        }

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

        const bool escHeld = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escHeld && !escWasPressed && playMode) {
            playMode = false;
            SetCursorMode(window, GLFW_CURSOR_NORMAL, false);
            mouseLookEnabled = false;
            mouseLookNeedsReset = true;

            // Snap back to the idle animation state immediately upon exiting via ESC
            currentPlayerAnimState = PlayerAnimState::Idle;
            playerAnimator.PlayAnimation(playerIdleAnim);
        }
        escWasPressed = escHeld;

        if (!playMode) {
            camera.ProcessKeyboard(w, s, a, d, deltaTime);

            constexpr float kVerticalSpeedMultiplier = 2.0f;
            const bool spaceHeldEditor = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            const bool ctrlHeldEditor = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            if (spaceHeldEditor) {
                camera.Position += glm::vec3(0.0f, 1.0f, 0.0f) * camera.GetMoveSpeed() * kVerticalSpeedMultiplier * deltaTime;
            }
            if (ctrlHeldEditor) {
                camera.Position -= glm::vec3(0.0f, 1.0f, 0.0f) * camera.GetMoveSpeed() * kVerticalSpeedMultiplier * deltaTime;
            }
        }

        physicsWorld.Step(deltaTime);

        glm::vec3 viewerPosition = camera.Position;

        if (playMode) {
            glm::vec3 wishDir(0.0f);
            if (w) wishDir += followCamera.GetForwardXZ();
            if (s) wishDir -= followCamera.GetForwardXZ();
            if (d) wishDir += followCamera.GetRightXZ();
            if (a) wishDir -= followCamera.GetRightXZ();

            // --- Face the player toward movement direction ---
            if (glm::length(wishDir) > 0.001f) {
                glm::vec3 facingDir = glm::normalize(wishDir);
                // atan2(x, z): this engine's forward is -Z (Camera's yaw=-90
                // gives front=(0,0,-1)), so this yields the correct yaw for
                // "facing where you're walking."
                float targetYaw = std::atan2(facingDir.x, facingDir.z);
                glm::quat targetRotation = glm::angleAxis(targetYaw, glm::vec3(0.0f, 1.0f, 0.0f));

                // Slerp toward the target facing instead of snapping, so
                // strafing/backward movement doesn't instantly flip the model.
                glm::quat& currentRotation = scene.Registry.get<Transform>(playerEntity).Rotation;
                constexpr float kTurnSpeed = 12.0f; // higher = snappier turning
                currentRotation = glm::slerp(currentRotation, targetRotation, glm::min(kTurnSpeed * deltaTime, 1.0f));
            }

            bool spaceHeld = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            bool jumpEdge = spaceHeld && !spaceWasPressed;
            spaceWasPressed = spaceHeld;

            characterController.Sprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            characterController.Update(deltaTime, wishDir, jumpEdge);

            // --- Locomotion animation state switch ---
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
        }

        // Only advance animation time while in play mode; otherwise tick by
        // 0.0f to freeze on whatever pose is currently showing (idle, after
        // the resets above).
        float frameTime = playMode ? deltaTime : 0.0f;
        playerAnimator.UpdateAnimation(frameTime);

        // --- Pass 1: render the scene into the HDR framebuffer -------------
        glBindFramebuffer(GL_FRAMEBUFFER, postProcess.hdrFBO);
        glViewport(0, 0, postProcess.fullWidth, postProcess.fullHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.35f, 0.35f, 0.38f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 activeView = playMode ? followCamera.GetViewMatrix() : camera.GetViewMatrix();
        const glm::mat4 activeProjection = playMode ? followCamera.GetProjectionMatrix(aspectRatio) : camera.GetProjectionMatrix(aspectRatio);
        const glm::vec3 activeCameraPos = playMode ? followCamera.Position : camera.Position;

        skybox.Render(activeView, activeProjection, activeCameraPos, -kDirLightDirection);

        if (!playMode) {
            gridRenderer.Render(camera, aspectRatio);
        }

        const glm::vec3 dirLightColor = ComputeSunLightColor(-kDirLightDirection);

        triangleShader.Bind();
        triangleShader.SetMat4("uView", activeView);
        triangleShader.SetMat4("uProjection", activeProjection);
        triangleShader.SetVec3("uViewPos", activeCameraPos);

        triangleShader.SetVec3("uDirLightDirection", kDirLightDirection);
        triangleShader.SetVec3("uDirLightColor", dirLightColor);
        triangleShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
        triangleShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));

        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
        }

        chunkManager.Update(viewerPosition, scene, physicsWorld);

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

        scriptEngine.CallUpdate(deltaTime);
        scriptEngine.CheckForReload(deltaTime);

        static float queryTimer = 0.0f;
        queryTimer += deltaTime;
        if (queryTimer > 2.0f) {
            queryTimer = 0.0f;
            auto nearby = spatialGrid.QueryRadius(viewerPosition, 20.0f);
            Log::Info("Spatial query: {} entities within 20 units of viewer", nearby.size());
        }

        auto view = scene.Registry.view<Transform, MeshRenderer>();
        for (auto entity : view) {
            auto [transform, renderer] = view.get<Transform, MeshRenderer>(entity);
            glm::mat4 worldMatrix = scene.GetWorldMatrix(entity);
            triangleShader.SetMat4("uModel", worldMatrix);

            if (entity == playerVisualEntity) {
                triangleShader.SetMat4Array("uBoneMatrices", playerAnimator.GetFinalBoneMatrices());
            }

            renderer.ModelRef->Draw(triangleShader, renderer.MaterialRef ? renderer.MaterialRef.get() : nullptr);
        }

        // --- Pass 2: bright-pass extraction (half-res) ----------------------
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

        // --- Pass 3: ping-pong Gaussian blur (half-res) ----------------------
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

        // --- Pass 4: composite HDR scene + bloom, tonemap, to the screen ----
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

        // --- Editor UI: drawn last, directly onto the composited backbuffer -
        editorUI.BeginFrame();
        editorUI.DrawMenuBar();
        editorUI.DrawSceneHierarchy(scene);
        editorUI.DrawInspector(scene);
        editorUI.DrawAssetBrowser(scene);
        editorUI.DrawPhysicsPanel(scene, physicsWorld);

        bool playModeBeforePanel = playMode;
        editorUI.DrawPlayerPanel(playMode, &characterController);
        if (playMode != playModeBeforePanel) {
            SetCursorMode(window, playMode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL, true);
            mouseLookEnabled = false;
            mouseLookNeedsReset = true;

            if (playMode) {
                characterController.SetPosition(kPlayerSpawnPosition);
                scene.Registry.get<Transform>(playerEntity).Position = kPlayerSpawnPosition;
                chunkManager.Update(kPlayerSpawnPosition, scene, physicsWorld);
            } else {
                // Snap back to the idle animation state immediately upon exiting via Editor UI
                currentPlayerAnimState = PlayerAnimState::Idle;
                playerAnimator.PlayAnimation(playerIdleAnim);
            }
        }

        editorUI.DrawViewportSettings(camera, gridRenderer);
        editorUI.DrawStatsOverlay();
        editorUI.Render();

        glfwSwapBuffers(window);
    }

    DestroyPostProcessTargets(postProcess);
    glDeleteVertexArrays(1, &fullscreenVAO);

    glfwDestroyWindow(window);
    editorUI.Shutdown();
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}
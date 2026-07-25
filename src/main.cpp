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
#include "rendering/Frustum.h"
#include "rendering/FrustumRenderer.h"

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
    bool* mouseLookDragged; // true once the mouse has moved noticeably since press —
                            // used to tell "click to select" apart from "drag to look"
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
    bool mouseLookDragged = false;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;

    Shader triangleShader("shaders/triangle.vert", "shaders/triangle.frag");
    Shader triangleInstancedShader("shaders/triangle_instanced.vert", "shaders/triangle.frag");

    Shader bloomThresholdShader("shaders/sky.vert", "shaders/bloom_threshold.frag");
    Shader bloomBlurShader("shaders/sky.vert", "shaders/bloom_blur.frag");
    Shader bloomCompositeShader("shaders/sky.vert", "shaders/bloom_composite.frag");

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
    scene.Registry.get<Transform>(playerEntity).Position = glm::vec3(0.0f, 1.0f, 0.0f);

    auto playerVisualEntity = scene.CreateEntity();
    auto& playerVisualTransform = scene.Registry.get<Transform>(playerVisualEntity);
    playerVisualTransform.Parent = playerEntity;
    playerVisualTransform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
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

    ChunkManager chunkManager(50.0f, 6);

    ScriptEngine scriptEngine;
    scriptEngine.Initialize(&scene);
    scriptEngine.RunScript("scripts/test.lua");

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

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
    int renderedEntityCount = 0;
    int culledEntityCount = 0;

    float lastFrameTime = 0.0f;
    bool altRWasPressed = false;
    bool spaceWasPressed = false;
    bool escWasPressed = false;
    bool deleteWasPressed = false;

    // --- Temporary profiling instrumentation ----------------------------
    // Logs a per-system frame-time breakdown once per second so you can see
    // where time is actually going (physics step, chunk streaming, spatial
    // grid rebuild, rigid body transform sync, culling+draw, bloom) instead
    // of only having one aggregate FPS number.
    float profileLogTimer = 0.0f;
    auto profileStart = []() { return std::chrono::high_resolution_clock::now(); };
    auto profileMs = [](auto start) {
        return std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - start).count();
    };

    WindowUserData userData{
        &camera, &followCamera, &playMode, &aspectRatio, &editorUI,
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

    chunkManager.Update(camera.Position, scene, physicsWorld);

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

            currentPlayerAnimState = PlayerAnimState::Idle;
            playerAnimator.PlayAnimation(playerIdleAnim);
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
            const bool spaceHeldEditor = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            const bool ctrlHeldEditor = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
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

        // --- Profiled: physics step -----------------------------------------
        auto t_physicsStep = profileStart();
        physicsWorld.Step(deltaTime);
        double ms_physicsStep = profileMs(t_physicsStep);

        glm::vec3 viewerPosition = camera.Position;

        if (playMode) {
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

            bool spaceHeld = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            bool jumpEdge = spaceHeld && !spaceWasPressed;
            spaceWasPressed = spaceHeld;

            characterController.Sprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            characterController.Update(deltaTime, wishDir, jumpEdge);

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
        const glm::mat4 activeViewProjection = activeProjection * activeView;

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

        // --- Profiled: spatial grid rebuild --------------------------------
        auto t_spatialGrid = profileStart();
        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
        }
        double ms_spatialGrid = profileMs(t_spatialGrid);

        // --- Profiled: chunk streaming --------------------------------------
        auto t_chunkUpdate = profileStart();
        chunkManager.Update(viewerPosition, scene, physicsWorld);
        double ms_chunkUpdate = profileMs(t_chunkUpdate);

        // --- Profiled: rigid body transform sync ----------------------------
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
        double ms_rigidBodySync = profileMs(t_rigidBodySync);

        scriptEngine.CallUpdate(deltaTime);
        scriptEngine.CheckForReload(deltaTime);

        static float queryTimer = 0.0f;
        queryTimer += deltaTime;
        if (queryTimer > 2.0f) {
            queryTimer = 0.0f;
            auto nearby = spatialGrid.QueryRadius(viewerPosition, 20.0f);
            Log::Info("Spatial query: {} entities within 20 units of viewer", nearby.size());
        }

        // --- Profiled: culling + draw (both instanced and non-instanced) ---
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

        // Query only entities near the camera instead of iterating the
        // entire registry — this is what makes maxRenderDistance actually
        // reduce per-frame CPU cost, rather than just skipping the draw
        // call after still paying for the world-matrix/culling math on
        // every entity in the scene regardless of distance.
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
            const bool insideFrustum = cullingFrustum.IntersectsSphere(worldCenter, worldRadius);

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

        // --- Profiled: bloom passes 2/3/4 -----------------------------------
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

        // --- Profiled: log the breakdown once per second --------------------
        profileLogTimer += deltaTime;
        if (profileLogTimer > 1.0f) {
            profileLogTimer = 0.0f;
            Log::Info("Profile ms — physics: {:.2f} chunk: {:.2f} grid: {:.2f} rbSync: {:.2f} cull+draw: {:.2f} bloom: {:.2f} total: {:.2f}",
                ms_physicsStep, ms_chunkUpdate, ms_spatialGrid, ms_rigidBodySync, ms_cullingAndDraw, ms_bloom,
                ms_physicsStep + ms_chunkUpdate + ms_spatialGrid + ms_rigidBodySync + ms_cullingAndDraw + ms_bloom);
        }

        // --- Editor UI: drawn last, directly onto the composited backbuffer -
        editorUI.BeginFrame();
        editorUI.DrawMenuBar();
        editorUI.DrawSceneHierarchy(scene);
        editorUI.DrawInspector(scene);
        editorUI.DrawAssetBrowser(scene);
        editorUI.DrawPhysicsPanel(scene, physicsWorld);
        editorUI.DrawGizmoToolbar();
        editorUI.DrawTransformGizmo(scene, physicsWorld, camera, aspectRatio);

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
                currentPlayerAnimState = PlayerAnimState::Idle;
                playerAnimator.PlayAnimation(playerIdleAnim);
            }
        }

        editorUI.DrawViewportSettings(camera, gridRenderer);
        editorUI.DrawCullingPanel(freezeCullingFrustum, maxRenderDistance, renderedEntityCount, culledEntityCount);
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
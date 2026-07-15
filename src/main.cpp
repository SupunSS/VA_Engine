#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "core/Log.h"
#include "core/Assert.h"
#include "core/memory/StackAllocator.h"
#include "core/memory/PoolAllocator.h"
#include "core/jobs/JobSystem.h"
#include <atomic>
#include <chrono>
#include "rendering/Shader.h"
#include "rendering/Camera.h"
#include "rendering/Texture.h"
#include "rendering/Model.h"
#include "scene/Scene.h"
#include "scene/Components.h"
#include "scene/SpatialGrid.h"
#include "scene/SceneLoader.h"
#include "scripting/ScriptEngine.h"
#include <glm/glm.hpp>
#include "scene/ChunkManager.h"
#include "editor/EditorUI.h"
#include <imgui.h>
#include "rendering/GridRenderer.h"
#include "physics/PhysicsWorld.h"
#include "rendering/Material.h"

namespace {
struct WindowUserData {
    Camera* camera;
    float* aspectRatio;
    EditorUI* editorUI;
    bool* mouseLookEnabled;
    bool* mouseLookNeedsReset;
    double* lastCursorX;
    double* lastCursorY;
    bool* isFullscreen;
    int* windowedX;
    int* windowedY;
    int* windowedWidth;
    int* windowedHeight;
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

void ToggleFullscreenWindow(GLFWwindow* window, WindowUserData* userData)
{
    if (userData == nullptr) {
        return;
    }

    if (*userData->isFullscreen) {
        glfwSetWindowMonitor(window, nullptr, *userData->windowedX, *userData->windowedY,
            *userData->windowedWidth, *userData->windowedHeight, GLFW_DONT_CARE);
        *userData->isFullscreen = false;
    } else {
        glfwGetWindowPos(window, userData->windowedX, userData->windowedY);
        glfwGetWindowSize(window, userData->windowedWidth, userData->windowedHeight);

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
        if (videoMode != nullptr) {
            glfwSetWindowMonitor(window, primaryMonitor, 0, 0, videoMode->width, videoMode->height,
                videoMode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, primaryMonitor, 0, 0, 1280, 720, GLFW_DONT_CARE);
        }

        *userData->isFullscreen = true;
    }

    const bool shouldLockCursor = *userData->mouseLookEnabled;
    SetCursorMode(window, shouldLockCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL, shouldLockCursor);
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

    glfwMaximizeWindow(window);  
    glfwShowWindow(window);       
    glfwFocusWindow(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

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
    bool isFullscreen = false;
    int windowedX = 0;
    int windowedY = 0;
    int windowedWidth = 1280;
    int windowedHeight = 720;

    Shader triangleShader("shaders/triangle.vert", "shaders/triangle.frag");

    auto defaultMaterial = std::make_shared<Material>();
    defaultMaterial->albedoTint = glm::vec3(0.5f, 0.5f, 0.5f);

    Scene scene;
    SpatialGrid spatialGrid(50.0f);
    PhysicsWorld physicsWorld;

    ChunkManager chunkManager(50.0f, 1); // 50-unit chunks, load 1 chunk radius around viewer

    ScriptEngine scriptEngine;
    scriptEngine.Initialize(&scene);
    scriptEngine.RunScript("scripts/test.lua");

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

    auto groundEntity = scene.CreateEntity();
    auto& groundTransform = scene.Registry.get<Transform>(groundEntity);
    groundTransform.Position = glm::vec3(0.0f, -1.0f, 0.0f);
    groundTransform.Scale = glm::vec3(10.0f, 0.25f, 10.0f);
    scene.Registry.emplace<MeshRenderer>(
    groundEntity,
    SceneLoader::GetOrLoadModel("models/cube.obj"),
    defaultMaterial
);
    scene.Registry.emplace<RigidBody>(
        groundEntity,
        physicsWorld.CreateBoxBody(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(10.0f, 0.25f, 10.0f), true),
        true,
        PhysicsShapeType::Box,
        glm::vec3(10.0f, 0.25f, 10.0f),
        0.5f
    );

    GridRenderer gridRenderer;
    float lastFrameTime = 0.0f;
    bool altRWasPressed = false;
    bool altEnterWasPressed = false;
    bool f11WasPressed = false;

    WindowUserData userData{ &camera, &aspectRatio, &editorUI, &mouseLookEnabled, &mouseLookNeedsReset, &lastCursorX, &lastCursorY, &isFullscreen, &windowedX, &windowedY, &windowedWidth, &windowedHeight };
    glfwSetWindowUserPointer(window, &userData);

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)xpos, (float)ypos);

    if (io.WantCaptureMouse) return;

    auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
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

    bool ctrlHeld = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (ctrlHeld) {
        auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
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

    SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
});

glfwSetDropCallback(window, [](GLFWwindow* win, int count, const char** paths) {
    auto* userData = static_cast<WindowUserData*>(glfwGetWindowUserPointer(win));
    if (userData != nullptr && userData->editorUI != nullptr) {
        userData->editorUI->QueueDroppedFiles(count, paths);
    }
});

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

        const bool altHeld = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        const bool rHeld = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        const bool enterHeld = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        const bool f11Held = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (altHeld && rHeld && !altRWasPressed) {
            editorUI.ToggleStatsOverlay();
        }
        if ((altHeld && enterHeld && !altEnterWasPressed) || (f11Held && !f11WasPressed)) {
            ToggleFullscreenWindow(window, &userData);
        }
        altRWasPressed = altHeld && rHeld;
        altEnterWasPressed = altHeld && enterHeld;
        f11WasPressed = f11Held;

        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        camera.ProcessKeyboard(w, s, a, d, deltaTime);

    
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gridRenderer.Render(camera, aspectRatio);

        triangleShader.Bind();

        triangleShader.SetMat4("uView", camera.GetViewMatrix());
        triangleShader.SetMat4("uProjection", camera.GetProjectionMatrix(aspectRatio));

        triangleShader.SetVec3("uViewPos", camera.Position);
        triangleShader.SetVec3("uDirLightDirection", glm::vec3(-0.3f, -1.0f, -0.3f));
        triangleShader.SetVec3("uDirLightColor", glm::vec3(0.6f, 0.6f, 0.55f));
        triangleShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
        triangleShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));

        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
        }

        chunkManager.Update(camera.Position, scene);

        physicsWorld.Step(deltaTime);

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
            auto nearby = spatialGrid.QueryRadius(camera.Position, 20.0f);
            Log::Info("Spatial query: {} entities within 20 units of camera", nearby.size());
        }

        auto view = scene.Registry.view<Transform, MeshRenderer>();
for (auto entity : view) {
    auto [transform, renderer] = view.get<Transform, MeshRenderer>(entity);
    glm::mat4 worldMatrix = scene.GetWorldMatrix(entity);
    triangleShader.SetMat4("uModel", worldMatrix);

    renderer.ModelRef->Draw(triangleShader, renderer.MaterialRef ? renderer.MaterialRef.get() : nullptr);
}

        editorUI.BeginFrame();
        editorUI.DrawMenuBar();
        editorUI.DrawSceneHierarchy(scene);
        editorUI.DrawInspector(scene);
        editorUI.DrawAssetBrowser(scene);
        editorUI.DrawPhysicsPanel(scene, physicsWorld);
        editorUI.DrawViewportSettings(camera, gridRenderer);
        editorUI.DrawStatsOverlay();
        editorUI.Render();

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    editorUI.Shutdown();
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}

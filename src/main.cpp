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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VA Engine", nullptr, nullptr);
    ENGINE_ASSERT(window != nullptr, "Failed to create GLFW window");

    glfwMakeContextCurrent(window);
    ENGINE_ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to initialize GLAD");
    Log::Info("OpenGL loaded: {}", (const char*)glGetString(GL_VERSION));

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    Shader triangleShader("shaders/triangle.vert", "shaders/triangle.frag");
    Texture cubeTexture("textures/test.png");

    Scene scene;
    SpatialGrid spatialGrid(50.0f);

    SceneLoader::LoadFromFile("scenes/test_scene.json", scene);

    ScriptEngine scriptEngine;
    scriptEngine.Initialize(&scene);
    scriptEngine.RunScript("scripts/test.lua");

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    float lastFrameTime = 0.0f;

    glfwSetWindowUserPointer(window, &camera);

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        static float lastX = 640.0f, lastY = 360.0f;
        static bool firstMouse = true;
        Camera* cam = static_cast<Camera*>(glfwGetWindowUserPointer(win));

        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
            firstMouse = true;
            return;
        }

        if (firstMouse) {
            lastX = (float)xpos;
            lastY = (float)ypos;
            firstMouse = false;
        }

        float xOffset = (float)xpos - lastX;
        float yOffset = lastY - (float)ypos;
        lastX = (float)xpos;
        lastY = (float)ypos;

        cam->ProcessMouseMovement(xOffset, yOffset);
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS)
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            else if (action == GLFW_RELEASE)
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    });

    Log::Info("Window created successfully");

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        glfwPollEvents();

        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        camera.ProcessKeyboard(w, s, a, d, deltaTime);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        triangleShader.Bind();

        triangleShader.SetMat4("uView", camera.GetViewMatrix());
        triangleShader.SetMat4("uProjection", camera.GetProjectionMatrix(1280.0f / 720.0f));

        triangleShader.SetVec3("uViewPos", camera.Position);
        triangleShader.SetVec3("uDirLightDirection", glm::vec3(-0.3f, -1.0f, -0.3f));
        triangleShader.SetVec3("uDirLightColor", glm::vec3(0.6f, 0.6f, 0.55f));
        triangleShader.SetVec3("uPointLightPos", glm::vec3(1.5f, 1.5f, 1.5f));
        triangleShader.SetVec3("uPointLightColor", glm::vec3(1.0f, 0.8f, 0.5f));

        cubeTexture.Bind(0);

        spatialGrid.Clear();
        auto posView = scene.Registry.view<Transform>();
        for (auto entity : posView) {
            spatialGrid.Insert(entity, posView.get<Transform>(entity).Position);
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
            renderer.ModelRef->Draw();
        }

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}
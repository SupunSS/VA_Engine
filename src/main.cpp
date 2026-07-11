#include <GLFW/glfw3.h>
#include "core/Log.h"
#include "core/Assert.h"

int main() {
    Log::Info("Engine starting up...");

    ENGINE_ASSERT(glfwInit(), "GLFW failed to initialize");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no OpenGL context yet, just a window

    GLFWwindow* window = glfwCreateWindow(1280, 720, "GTA Engine - Phase 0", nullptr, nullptr);
    ENGINE_ASSERT(window != nullptr, "Failed to create GLFW window");

    Log::Info("Window created successfully");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    Log::Info("Engine shut down cleanly");
    return 0;
}
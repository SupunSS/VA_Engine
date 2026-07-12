#include <GLFW/glfw3.h>
#include "core/Log.h"
#include "core/Assert.h"
#include "core/memory/StackAllocator.h"
#include "core/memory/PoolAllocator.h"
#include "core/jobs/JobSystem.h"
#include <atomic>
#include <chrono>

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
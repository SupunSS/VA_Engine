#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "Window.h"
#include "AppState.h"
#include "WindowUtils.h"
#include "../core/Assert.h"
#include <imgui.h>
#include <cmath>

GLFWwindow* CreateMainWindow()
{
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

    return window;
}

void InstallWindowCallbacks(GLFWwindow* window, AppState& app)
{
    glfwSetWindowUserPointer(window, &app);

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent((float)xpos, (float)ypos);

        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));
        InputState& in = app->input;

        if (app->play.playMode) {
            if (in.mouseLookNeedsReset) {
                in.lastCursorX = xpos;
                in.lastCursorY = ypos;
                in.mouseLookNeedsReset = false;
            }
            float xOffset = (float)xpos - (float)in.lastCursorX;
            float yOffset = (float)in.lastCursorY - (float)ypos;
            in.lastCursorX = xpos;
            in.lastCursorY = ypos;
            if (app->play.insideVehicle) {
                app->world.vehicleCamera->ProcessMouseMovement(xOffset, yOffset);
            } else {
                app->world.followCamera->ProcessMouseMovement(xOffset, yOffset);
            }
            return;
        }

        if (io.WantCaptureMouse) return;

        if (!in.mouseLookEnabled) {
            in.mouseLookNeedsReset = true;
            return;
        }

        if (in.mouseLookNeedsReset) {
            in.lastCursorX = xpos;
            in.lastCursorY = ypos;
            in.mouseLookNeedsReset = false;
        }

        float xOffset = (float)xpos - (float)in.lastCursorX;
        float yOffset = (float)in.lastCursorY - (float)ypos;
        in.lastCursorX = xpos;
        in.lastCursorY = ypos;

        if (std::abs(xOffset) > 1.0f || std::abs(yOffset) > 1.0f) {
            in.mouseLookDragged = true;
        }

        app->world.camera->ProcessMouseMovement(xOffset, yOffset);
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);

        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));
        InputState& in = app->input;

        if (app->play.playMode) return;

        if (io.WantCaptureMouse) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
                in.mouseLookEnabled = false;
                SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
            }
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                in.mouseLookEnabled = true;
                in.mouseLookNeedsReset = true;
                in.mouseLookDragged = false;
                SetCursorMode(win, GLFW_CURSOR_DISABLED, true);
            } else if (action == GLFW_RELEASE) {
                in.mouseLookEnabled = false;
                SetCursorMode(win, GLFW_CURSOR_NORMAL, false);

                EditorUI& editorUI = *app->editorObjects.ui;
                if (!in.mouseLookDragged && !editorUI.IsGizmoActive()) {
                    double mouseX = 0.0, mouseY = 0.0;
                    glfwGetCursorPos(win, &mouseX, &mouseY);
                    int fbWidth = 0, fbHeight = 0;
                    glfwGetFramebufferSize(win, &fbWidth, &fbHeight);
                    editorUI.HandleViewportClick(
                        *app->world.scene, *app->world.camera, app->frame.aspectRatio,
                        mouseX, mouseY, fbWidth, fbHeight);
                }
            }
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent((float)xoffset, (float)yoffset);

        if (io.WantCaptureMouse) return;

        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));

        if (app->play.playMode) {
            if (app->play.insideVehicle) {
                app->world.vehicleCamera->ProcessScroll((float)yoffset);
            } else {
                app->world.followCamera->ProcessScroll((float)yoffset);
            }
            return;
        }

        bool ctrlHeld = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        if (ctrlHeld) {
            Camera& camera = *app->world.camera;
            float increment = (float)yoffset * camera.GetMoveSpeed() * 0.1f;
            camera.AdjustMoveSpeed(increment);
        }
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int newWidth, int newHeight) {
        if (newHeight == 0) return;
        glViewport(0, 0, newWidth, newHeight);

        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));
        app->frame.aspectRatio = (float)newWidth / (float)newHeight;
    });

    glfwSetWindowFocusCallback(window, [](GLFWwindow* win, int focused) {
        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));
        if (app == nullptr) {
            return;
        }

        if (!focused) {
            app->input.mouseLookEnabled = false;
            app->input.mouseLookNeedsReset = true;
        }

        if (!app->play.playMode) {
            SetCursorMode(win, GLFW_CURSOR_NORMAL, false);
        }
    });

    glfwSetDropCallback(window, [](GLFWwindow* win, int count, const char** paths) {
        auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(win));
        if (app != nullptr && app->editorObjects.ui) {
            app->editorObjects.ui->QueueDroppedFiles(count, paths);
        }
    });
}

void ShowMainWindow(GLFWwindow* window)
{
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
}
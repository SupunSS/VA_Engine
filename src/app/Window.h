#pragma once

struct GLFWwindow;
struct AppState;

// Initializes GLFW, creates the (initially hidden) main window, makes its GL
// context current and loads GLAD.
GLFWwindow* CreateMainWindow();

// Stores `app` as the window's user pointer and installs the mouse, scroll,
// resize, focus and file-drop callbacks. Call AFTER EditorUI::Initialize so
// ImGui's own callback chain is set up first, same as before.
void InstallWindowCallbacks(GLFWwindow* window, AppState& app);

// Shows, maximizes and focuses the window and sets up cursor/raw-mouse input.
void ShowMainWindow(GLFWwindow* window);
#pragma once

struct GLFWwindow;

// Sets the GLFW cursor mode, optionally re-centering the cursor in the window.
void SetCursorMode(GLFWwindow* window, int cursorMode, bool centerCursor);
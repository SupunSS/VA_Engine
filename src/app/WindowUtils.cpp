#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "WindowUtils.h"
#include <GLFW/glfw3.h>

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
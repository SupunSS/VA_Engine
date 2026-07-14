#pragma once
#include <GLFW/glfw3.h>
#include "../scene/Scene.h"
#include <entt/entt.hpp>

class EditorUI {
public:
    void Initialize(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void Render();

    void DrawSceneHierarchy(Scene& scene);
    void DrawInspector(Scene& scene);
    void DrawAssetBrowser();

    entt::entity SelectedEntity = entt::null;
};
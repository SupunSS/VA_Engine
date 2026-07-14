#pragma once
#include <GLFW/glfw3.h>
#include "../scene/Scene.h"
#include "../rendering/Camera.h"
#include "../rendering/GridRenderer.h"
#include <entt/entt.hpp>

class EditorUI {
public:
    void Initialize(GLFWwindow* window);
    void Shutdown();

    void BeginFrame();
    void Render();

    void DrawMenuBar(); // new — houses the toggles
    void DrawSceneHierarchy(Scene& scene);
    void DrawInspector(Scene& scene);
    void DrawAssetBrowser();
    void DrawViewportSettings(Camera& camera, GridRenderer& gridRenderer);

    entt::entity SelectedEntity = entt::null;

    // Per-panel visibility — toggled from the menu bar.
    bool ShowSceneHierarchy = true;
    bool ShowInspector = true;
    bool ShowAssetBrowser = true;
    bool ShowViewportSettings = true;
};
#include "EditorUI.h"
#include "../scene/Components.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <unordered_set>

void EditorUI::Initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void EditorUI::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void EditorUI::Render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EditorUI::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &ShowSceneHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &ShowInspector);
            ImGui::MenuItem("Asset Browser", nullptr, &ShowAssetBrowser);
            ImGui::MenuItem("Viewport Settings", nullptr, &ShowViewportSettings);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void EditorUI::DrawSceneHierarchy(Scene& scene) {
    if (!ShowSceneHierarchy) return;
    ImGui::Begin("Scene Hierarchy", &ShowSceneHierarchy);

    auto view = scene.Registry.view<Transform>();
    for (auto entity : view) {
        std::string label = "Entity " + std::to_string(static_cast<uint32_t>(entity));

        bool isSelected = (entity == SelectedEntity);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            SelectedEntity = entity;
        }
    }

    ImGui::End();
}

void EditorUI::DrawInspector(Scene& scene) {
    if (!ShowInspector) return;
    ImGui::Begin("Inspector", &ShowInspector);

    if (SelectedEntity != entt::null && scene.Registry.valid(SelectedEntity)) {
        if (scene.Registry.all_of<Transform>(SelectedEntity)) {
            auto& transform = scene.Registry.get<Transform>(SelectedEntity);

            ImGui::Text("Transform");
            ImGui::DragFloat3("Position", &transform.Position.x, 0.1f);
            ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
        }

        if (scene.Registry.all_of<ChunkId>(SelectedEntity)) {
            auto& chunk = scene.Registry.get<ChunkId>(SelectedEntity);
            ImGui::Text("Chunk: (%d, %d)", chunk.x, chunk.z);
        }

        if (scene.Registry.all_of<MeshRenderer>(SelectedEntity)) {
            ImGui::Text("Has MeshRenderer: Yes");
        }
    } else {
        ImGui::Text("No entity selected");
    }

    ImGui::End();
}

void EditorUI::DrawAssetBrowser() {
    if (!ShowAssetBrowser) return;
    ImGui::Begin("Asset Browser", &ShowAssetBrowser);

    static std::string currentPath = ".";

    // Folders that belong to the engine/build/toolchain, never real assets.
    static const std::unordered_set<std::string> hiddenDirs = {
        "src", "build", "vcpkg", ".git", ".vs", "cmake-build-debug", "cmake-build-release"
    };

    // Only files with these extensions count as "assets" worth showing.
    static const std::unordered_set<std::string> assetExtensions = {
        ".obj", ".fbx", ".gltf", ".glb",           // models
        ".png", ".jpg", ".jpeg", ".tga", ".hdr",   // textures
        ".json",                                    // scenes
        ".lua",                                      // scripts
        ".vert", ".frag", ".glsl"                   // shaders
    };

    if (ImGui::Button("Up")) {
        std::filesystem::path p(currentPath);
        if (p.has_parent_path()) currentPath = p.parent_path().string();
    }
    ImGui::SameLine();
    ImGui::Text("%s", currentPath.c_str());

    ImGui::Separator();

    if (std::filesystem::exists(currentPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
            std::string name = entry.path().filename().string();

            if (!name.empty() && name[0] == '.') continue;

            if (entry.is_directory()) {
                if (hiddenDirs.count(name)) continue;
                if (ImGui::Selectable(("[DIR] " + name).c_str())) {
                    currentPath = entry.path().string();
                }
            } else {
                std::string ext = entry.path().extension().string();
                if (!assetExtensions.count(ext)) continue;
                ImGui::Text("      %s", name.c_str());
            }
        }
    }

    ImGui::End();
}

void EditorUI::DrawViewportSettings(Camera& camera, GridRenderer& gridRenderer) {
    if (!ShowViewportSettings) return;
    ImGui::Begin("Viewport Settings", &ShowViewportSettings);

    float speed = camera.GetMoveSpeed();
    if (ImGui::SliderFloat("Camera Speed", &speed, 0.5f, 50.0f, "%.1f")) {
        camera.SetMoveSpeed(speed);
    }
    ImGui::TextDisabled("Ctrl + Scroll to adjust in viewport");

    ImGui::Separator();
    ImGui::Checkbox("Show Grid", &gridRenderer.Visible);

    ImGui::End();
}
#include "EditorUI.h"
#include "../scene/Components.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>

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

void EditorUI::DrawSceneHierarchy(Scene& scene) {
    ImGui::Begin("Scene Hierarchy");

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
    ImGui::Begin("Inspector");

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
    ImGui::Begin("Asset Browser");

    static std::string currentPath = ".";

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
            if (entry.is_directory()) {
                if (ImGui::Selectable(("[DIR] " + name).c_str())) {
                    currentPath = entry.path().string();
                }
            } else {
                ImGui::Text("      %s", name.c_str());
            }
        }
    }

    ImGui::End();
}
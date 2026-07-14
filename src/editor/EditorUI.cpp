#include "EditorUI.h"
#include "../core/Log.h"
#include "../rendering/Texture.h"
#include "../scene/Components.h"
#include "../scene/SceneLoader.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <unordered_set>

namespace {
constexpr const char* kAssetPayloadType = "VA_ASSET_PATH";

const std::array<const char*, 8> kAssetFolders = {
    "models",
    "textures",
    "materials",
    "scenes",
    "scripts",
    "shaders",
    "audio",
    "prefabs"
};

const std::unordered_set<std::string> kHiddenDirs = {
    "src",
    "build",
    "vcpkg",
    ".git",
    ".vs",
    "cmake-build-debug",
    "cmake-build-release"
};

const std::unordered_set<std::string> kAssetExtensions = {
    ".obj", ".fbx", ".gltf", ".glb",
    ".png", ".jpg", ".jpeg", ".tga", ".hdr", ".bmp",
    ".json",
    ".lua",
    ".vert", ".frag", ".glsl", ".comp", ".geom",
    ".wav", ".mp3", ".ogg",
    ".mat", ".material",
    ".prefab"
};

const std::unordered_set<std::string> kModelExtensions = {
    ".obj", ".fbx", ".gltf", ".glb"
};

const std::unordered_set<std::string> kTextureExtensions = {
    ".png", ".jpg", ".jpeg", ".tga", ".hdr", ".bmp"
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string GetLowerExtension(const std::filesystem::path& path) {
    return ToLower(path.extension().string());
}

bool IsHiddenDirectory(const std::filesystem::path& path) {
    return kHiddenDirs.contains(ToLower(path.filename().string()));
}

bool IsSupportedAsset(const std::filesystem::path& path) {
    return kAssetExtensions.contains(GetLowerExtension(path));
}

bool IsModelAsset(const std::filesystem::path& path) {
    return kModelExtensions.contains(GetLowerExtension(path));
}

bool IsTextureAsset(const std::filesystem::path& path) {
    return kTextureExtensions.contains(GetLowerExtension(path));
}

bool HasInvalidAssetNameCharacter(const std::string& name) {
    return name.find_first_of("\\/<>:\"|?*") != std::string::npos;
}

std::filesystem::path GetDefaultFolderForAsset(const std::filesystem::path& path) {
    const std::string extension = GetLowerExtension(path);

    if (kModelExtensions.contains(extension)) return "models";
    if (kTextureExtensions.contains(extension)) return "textures";
    if (extension == ".json") return "scenes";
    if (extension == ".lua") return "scripts";
    if (extension == ".vert" || extension == ".frag" || extension == ".glsl" ||
        extension == ".comp" || extension == ".geom") {
        return "shaders";
    }
    if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") return "audio";
    if (extension == ".mat" || extension == ".material") return "materials";
    if (extension == ".prefab") return "prefabs";

    return {};
}
}

void EditorUI::Initialize(GLFWwindow* window) {
    m_projectRoot = std::filesystem::current_path();
    m_currentAssetPath = m_projectRoot;
    EnsureAssetDirectories();

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

void EditorUI::QueueDroppedFiles(int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        if (paths[i] != nullptr) {
            m_pendingDroppedFiles.emplace_back(paths[i]);
        }
    }
}

void EditorUI::EnsureAssetDirectories() {
    if (m_projectRoot.empty()) {
        m_projectRoot = std::filesystem::current_path();
    }
    if (m_currentAssetPath.empty()) {
        m_currentAssetPath = m_projectRoot;
    }

    std::error_code error;
    for (const char* folder : kAssetFolders) {
        std::filesystem::create_directories(m_projectRoot / folder, error);
        if (error) {
            Log::Warn("Failed to create asset folder {}: {}", folder, error.message());
            error.clear();
        }
    }
}

void EditorUI::ImportPendingDroppedFiles() {
    if (m_pendingDroppedFiles.empty()) {
        return;
    }

    EnsureAssetDirectories();

    int imported = 0;
    int skipped = 0;

    for (const auto& droppedPath : m_pendingDroppedFiles) {
        std::error_code error;
        std::filesystem::path sourcePath = std::filesystem::absolute(droppedPath, error).lexically_normal();
        if (error || !std::filesystem::exists(sourcePath, error)) {
            ++skipped;
            error.clear();
            continue;
        }

        const bool isDirectory = std::filesystem::is_directory(sourcePath, error);
        if (error) {
            ++skipped;
            error.clear();
            continue;
        }

        if (!isDirectory && !IsSupportedAsset(sourcePath)) {
            ++skipped;
            continue;
        }

        std::filesystem::path destinationDirectory = GetImportDestinationFor(sourcePath);
        std::filesystem::create_directories(destinationDirectory, error);
        if (error) {
            ++skipped;
            Log::Warn("Failed to create import destination {}: {}", destinationDirectory.string(), error.message());
            error.clear();
            continue;
        }

        std::filesystem::path destinationPath = MakeUniqueDestination(destinationDirectory / sourcePath.filename());
        if (isDirectory) {
            std::filesystem::copy(
                sourcePath,
                destinationPath,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing,
                error
            );
        } else {
            std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, error);
        }

        if (error) {
            ++skipped;
            Log::Warn("Failed to import asset {}: {}", sourcePath.string(), error.message());
            error.clear();
            continue;
        }

        ++imported;
    }

    if (imported > 0) {
        m_assetStatusMessage = "Imported " + std::to_string(imported) + " asset";
        if (imported != 1) {
            m_assetStatusMessage += "s";
        }
        if (skipped > 0) {
            m_assetStatusMessage += ", skipped " + std::to_string(skipped);
        }
    } else {
        m_assetStatusMessage = "No supported assets imported";
    }

    m_pendingDroppedFiles.clear();
}

std::filesystem::path EditorUI::GetImportDestinationFor(const std::filesystem::path& sourcePath) const {
    std::error_code error;
    const bool isDirectory = std::filesystem::is_directory(sourcePath, error);
    if (m_currentAssetPath == m_projectRoot) {
        if (isDirectory) {
            return m_projectRoot / "prefabs";
        }

        std::filesystem::path defaultFolder = GetDefaultFolderForAsset(sourcePath);
        if (!defaultFolder.empty()) {
            return m_projectRoot / defaultFolder;
        }
    }

    if (CanCreateAssetFolderInCurrentPath()) {
        return m_currentAssetPath;
    }

    return m_projectRoot / "prefabs";
}

std::filesystem::path EditorUI::MakeUniqueDestination(const std::filesystem::path& destinationPath) const {
    std::error_code error;
    if (!std::filesystem::exists(destinationPath, error)) {
        return destinationPath;
    }

    const std::filesystem::path parentPath = destinationPath.parent_path();
    const std::string stem = destinationPath.stem().string();
    const std::string extension = destinationPath.extension().string();

    for (int i = 1; i < 1000; ++i) {
        std::filesystem::path candidate = parentPath / (stem + "_" + std::to_string(i) + extension);
        if (!std::filesystem::exists(candidate, error)) {
            return candidate;
        }
        error.clear();
    }

    return parentPath / (stem + "_copy" + extension);
}

std::string EditorUI::GetDisplayPath(const std::filesystem::path& path) const {
    std::error_code error;
    std::filesystem::path relativePath = std::filesystem::relative(path, m_projectRoot, error);
    if (!error) {
        if (relativePath.empty() || relativePath == ".") {
            return ".";
        }
        return relativePath.string();
    }

    return path.string();
}

std::filesystem::path EditorUI::NormalizePath(const std::filesystem::path& path) const {
    std::error_code error;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, error);
    if (error) {
        return path.lexically_normal();
    }
    return absolutePath.lexically_normal();
}

bool EditorUI::IsSameOrChildPath(const std::filesystem::path& assetPath, const std::filesystem::path& rootPath) const {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    const std::filesystem::path normalizedRoot = NormalizePath(rootPath);
    const std::filesystem::path relativePath = normalizedPath.lexically_relative(normalizedRoot);

    if (relativePath.empty()) {
        return false;
    }

    auto firstPart = relativePath.begin();
    return firstPart == relativePath.end() || *firstPart != "..";
}

bool EditorUI::IsManagedAssetRoot(const std::filesystem::path& assetPath) const {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    for (const char* folder : kAssetFolders) {
        if (normalizedPath == NormalizePath(m_projectRoot / folder)) {
            return true;
        }
    }
    return false;
}

bool EditorUI::IsInsideManagedAssetRoot(const std::filesystem::path& assetPath, bool allowRoot) const {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    for (const char* folder : kAssetFolders) {
        const std::filesystem::path rootPath = NormalizePath(m_projectRoot / folder);
        if (normalizedPath == rootPath) {
            return allowRoot;
        }
        if (IsSameOrChildPath(normalizedPath, rootPath)) {
            return true;
        }
    }
    return false;
}

bool EditorUI::CanCreateAssetFolderInCurrentPath() const {
    return IsInsideManagedAssetRoot(m_currentAssetPath, true);
}

bool EditorUI::CanModifyAssetPath(const std::filesystem::path& assetPath) const {
    std::error_code error;
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    if (!std::filesystem::exists(normalizedPath, error) || error) {
        return false;
    }

    if (!IsInsideManagedAssetRoot(normalizedPath, false)) {
        return false;
    }

    if (std::filesystem::is_directory(normalizedPath, error)) {
        return !error && !IsHiddenDirectory(normalizedPath);
    }

    return IsSupportedAsset(normalizedPath);
}

void EditorUI::EraseTextureCacheForPath(const std::filesystem::path& assetPath) {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    for (auto it = m_textureCache.begin(); it != m_textureCache.end();) {
        const std::filesystem::path cachedPath = NormalizePath(it->first);
        if (cachedPath == normalizedPath || IsSameOrChildPath(cachedPath, normalizedPath)) {
            it = m_textureCache.erase(it);
        } else {
            ++it;
        }
    }
}

void EditorUI::BeginRenameAsset(const std::filesystem::path& assetPath) {
    if (!CanModifyAssetPath(assetPath)) {
        m_assetStatusMessage = "Only files and folders inside asset folders can be renamed";
        return;
    }

    m_isRenamingAsset = true;
    m_renamingAssetPath = NormalizePath(assetPath);

    const std::string currentName = m_renamingAssetPath.filename().string();
    std::strncpy(m_renameAssetName, currentName.c_str(), sizeof(m_renameAssetName) - 1);
    m_renameAssetName[sizeof(m_renameAssetName) - 1] = '\0';
}

bool EditorUI::RenameAsset(const std::filesystem::path& assetPath, const std::string& newName) {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    if (!CanModifyAssetPath(normalizedPath)) {
        m_assetStatusMessage = "Only files and folders inside asset folders can be renamed";
        return false;
    }

    if (newName.empty() || newName == "." || newName == ".." || HasInvalidAssetNameCharacter(newName)) {
        m_assetStatusMessage = "Enter a valid asset name";
        return false;
    }

    std::error_code error;
    const bool isDirectory = std::filesystem::is_directory(normalizedPath, error);
    if (error) {
        m_assetStatusMessage = "Could not inspect asset";
        return false;
    }

    std::filesystem::path finalName = newName;
    if (!isDirectory && finalName.extension().empty()) {
        finalName += normalizedPath.extension().string();
    }

    if (!isDirectory && !IsSupportedAsset(finalName)) {
        m_assetStatusMessage = "Rename would make this an unsupported asset type";
        return false;
    }

    const std::filesystem::path destinationPath = NormalizePath(normalizedPath.parent_path() / finalName);
    if (!IsInsideManagedAssetRoot(destinationPath, false)) {
        m_assetStatusMessage = "Asset must stay inside an asset folder";
        return false;
    }

    if (std::filesystem::exists(destinationPath, error)) {
        m_assetStatusMessage = "An asset with that name already exists";
        return false;
    }

    std::filesystem::rename(normalizedPath, destinationPath, error);
    if (error) {
        m_assetStatusMessage = "Could not rename asset";
        Log::Warn("Failed to rename asset {} to {}: {}", normalizedPath.string(), destinationPath.string(), error.message());
        return false;
    }

    EraseTextureCacheForPath(normalizedPath);
    m_isRenamingAsset = false;
    m_renamingAssetPath.clear();
    m_renameAssetName[0] = '\0';
    m_assetStatusMessage = "Renamed asset to " + destinationPath.filename().string();
    return true;
}

bool EditorUI::DeleteAsset(const std::filesystem::path& assetPath) {
    const std::filesystem::path normalizedPath = NormalizePath(assetPath);
    if (!CanModifyAssetPath(normalizedPath)) {
        m_assetStatusMessage = "Only files and folders inside asset folders can be deleted";
        return false;
    }

    std::error_code error;
    const bool isDirectory = std::filesystem::is_directory(normalizedPath, error);
    if (error) {
        m_assetStatusMessage = "Could not inspect asset";
        return false;
    }

    if (isDirectory) {
        std::filesystem::remove_all(normalizedPath, error);
    } else {
        std::filesystem::remove(normalizedPath, error);
    }

    if (error) {
        m_assetStatusMessage = "Could not delete asset";
        Log::Warn("Failed to delete asset {}: {}", normalizedPath.string(), error.message());
        return false;
    }

    EraseTextureCacheForPath(normalizedPath);
    if (m_isRenamingAsset && NormalizePath(m_renamingAssetPath) == normalizedPath) {
        m_isRenamingAsset = false;
        m_renamingAssetPath.clear();
    }

    m_assetStatusMessage = "Deleted " + normalizedPath.filename().string();
    return true;
}

bool EditorUI::AddAssetToScene(Scene& scene, const std::filesystem::path& assetPath) {
    if (!IsModelAsset(assetPath)) {
        m_assetStatusMessage = "Only model assets can be added to the scene right now";
        return false;
    }

    std::error_code error;
    std::filesystem::path normalizedPath = std::filesystem::absolute(assetPath, error).lexically_normal();
    if (error || !std::filesystem::exists(normalizedPath, error)) {
        m_assetStatusMessage = "Model file was not found";
        return false;
    }

    auto model = SceneLoader::GetOrLoadModel(normalizedPath.string());
    entt::entity entity = scene.CreateEntity();
    scene.Registry.emplace<MeshRenderer>(entity, model);
    SelectedEntity = entity;

    m_assetStatusMessage = "Added " + normalizedPath.filename().string() + " to the scene";
    return true;
}

bool EditorUI::ApplyTextureToSelectedEntity(Scene& scene, const std::filesystem::path& assetPath) {
    if (!IsTextureAsset(assetPath)) {
        m_assetStatusMessage = "Only texture assets can be assigned as textures";
        return false;
    }

    if (SelectedEntity == entt::null || !scene.Registry.valid(SelectedEntity) ||
        !scene.Registry.all_of<MeshRenderer>(SelectedEntity)) {
        m_assetStatusMessage = "Select a mesh entity before assigning a texture";
        return false;
    }

    auto texture = GetOrLoadTexture(assetPath);
    if (!texture) {
        return false;
    }

    auto& renderer = scene.Registry.get<MeshRenderer>(SelectedEntity);
    renderer.TextureRef = texture;

    m_assetStatusMessage = "Assigned " + assetPath.filename().string() + " to selected entity";
    return true;
}

std::shared_ptr<Texture> EditorUI::GetOrLoadTexture(const std::filesystem::path& assetPath) {
    std::error_code error;
    std::filesystem::path normalizedPath = std::filesystem::absolute(assetPath, error).lexically_normal();
    if (error || !std::filesystem::exists(normalizedPath, error)) {
        m_assetStatusMessage = "Texture file was not found";
        return nullptr;
    }

    const std::string key = normalizedPath.string();
    auto it = m_textureCache.find(key);
    if (it != m_textureCache.end()) {
        return it->second;
    }

    auto texture = std::make_shared<Texture>(key);
    m_textureCache[key] = texture;
    return texture;
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

    ImGui::BeginChild("SceneHierarchyDropTarget", ImVec2(0.0f, 0.0f), false);

    auto view = scene.Registry.view<Transform>();
    for (auto entity : view) {
        std::string label = "Entity " + std::to_string(static_cast<uint32_t>(entity));

        bool isSelected = (entity == SelectedEntity);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            SelectedEntity = entity;
        }
    }

    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetPayloadType)) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path != nullptr) {
                AddAssetToScene(scene, path);
            }
        }
        ImGui::EndDragDropTarget();
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
            const auto& renderer = scene.Registry.get<MeshRenderer>(SelectedEntity);
            ImGui::Text("Has MeshRenderer: Yes");
            ImGui::Text("Texture: %s", renderer.TextureRef ? "Custom" : "Default");
        }
    } else {
        ImGui::Text("No entity selected");
    }

    ImGui::End();
}

void EditorUI::DrawAssetBrowser(Scene& scene) {
    if (!ShowAssetBrowser) return;
    ImGui::Begin("Asset Browser", &ShowAssetBrowser);

    EnsureAssetDirectories();
    ImportPendingDroppedFiles();

    const bool atProjectRoot = m_currentAssetPath == m_projectRoot;
    if (!atProjectRoot && ImGui::Button("Up")) {
        m_currentAssetPath = m_currentAssetPath.parent_path();
    } else if (atProjectRoot) {
        ImGui::BeginDisabled();
        ImGui::Button("Up");
        ImGui::EndDisabled();
    }

    const bool canCreateFolder = CanCreateAssetFolderInCurrentPath();
    if (!canCreateFolder && m_isCreatingFolder) {
        m_isCreatingFolder = false;
        m_newFolderName[0] = '\0';
    }

    ImGui::SameLine();
    if (!canCreateFolder) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("New Folder")) {
        m_isCreatingFolder = true;
        m_newFolderName[0] = '\0';
    }
    if (!canCreateFolder) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::Text("%s", GetDisplayPath(m_currentAssetPath).c_str());

    if (m_isCreatingFolder) {
        ImGui::SetNextItemWidth(180.0f);
        const bool enterPressed = ImGui::InputText(
            "##NewFolderName",
            m_newFolderName,
            sizeof(m_newFolderName),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::SameLine();

        const bool createClicked = ImGui::Button("Create");
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_isCreatingFolder = false;
            m_newFolderName[0] = '\0';
        }

        if (enterPressed || createClicked) {
            const std::string folderName = m_newFolderName;
            if (!CanCreateAssetFolderInCurrentPath()) {
                m_assetStatusMessage = "Open an asset folder before creating folders";
                m_isCreatingFolder = false;
            } else if (folderName.empty()) {
                m_assetStatusMessage = "Folder name is empty";
            } else if (HasInvalidAssetNameCharacter(folderName)) {
                m_assetStatusMessage = "Folder name contains invalid characters";
            } else {
                std::error_code error;
                std::filesystem::create_directories(m_currentAssetPath / folderName, error);
                if (error) {
                    m_assetStatusMessage = "Could not create folder";
                    Log::Warn("Failed to create folder {}: {}", folderName, error.message());
                } else {
                    m_assetStatusMessage = "Created folder " + folderName;
                    m_isCreatingFolder = false;
                    m_newFolderName[0] = '\0';
                }
            }
        }
    }

    if (!m_assetStatusMessage.empty()) {
        ImGui::TextDisabled("%s", m_assetStatusMessage.c_str());
    }

    ImGui::Separator();

    std::error_code error;
    std::vector<std::filesystem::directory_entry> entries;
    for (std::filesystem::directory_iterator it(m_currentAssetPath, error), end; it != end && !error; it.increment(error)) {
        entries.push_back(*it);
    }

    if (error) {
        ImGui::Text("Could not read folder");
        Log::Warn("Failed to read asset folder {}: {}", m_currentAssetPath.string(), error.message());
    } else {
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            std::error_code lhsError;
            std::error_code rhsError;
            const bool lhsIsDirectory = lhs.is_directory(lhsError);
            const bool rhsIsDirectory = rhs.is_directory(rhsError);
            if (lhsIsDirectory != rhsIsDirectory) {
                return lhsIsDirectory;
            }
            return ToLower(lhs.path().filename().string()) < ToLower(rhs.path().filename().string());
        });

        for (const auto& entry : entries) {
            DrawAssetEntry(scene, entry);
        }
    }

    DrawDeleteAssetPopup();

    ImGui::End();
}

void EditorUI::DrawAssetEntry(Scene& scene, const std::filesystem::directory_entry& entry) {
    std::error_code error;
    const std::filesystem::path path = entry.path();
    const std::string name = path.filename().string();
    const bool isDirectory = entry.is_directory(error);

    if (name.empty() || name[0] == '.') {
        return;
    }

    if (error) {
        return;
    }

    if (isDirectory) {
        if (IsHiddenDirectory(path)) {
            return;
        }

        if (NormalizePath(path.parent_path()) == NormalizePath(m_projectRoot) && !IsManagedAssetRoot(path)) {
            return;
        }
    } else if (!IsSupportedAsset(path)) {
        return;
    }

    if (m_isRenamingAsset && NormalizePath(m_renamingAssetPath) == NormalizePath(path)) {
        ImGui::PushID(path.string().c_str());
        ImGui::SetNextItemWidth(220.0f);
        const bool enterPressed = ImGui::InputText(
            "##RenameAsset",
            m_renameAssetName,
            sizeof(m_renameAssetName),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::SameLine();
        const bool saveClicked = ImGui::Button("Save");
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_isRenamingAsset = false;
            m_renamingAssetPath.clear();
            m_renameAssetName[0] = '\0';
        }
        if (enterPressed || saveClicked) {
            RenameAsset(path, m_renameAssetName);
        }
        ImGui::PopID();
        return;
    }

    if (isDirectory) {
        if (ImGui::Selectable(("[DIR] " + name).c_str())) {
            m_currentAssetPath = path;
        }

        if (ImGui::BeginPopupContextItem()) {
            if (CanModifyAssetPath(path)) {
                if (ImGui::MenuItem("Rename")) {
                    BeginRenameAsset(path);
                }
                if (ImGui::MenuItem("Delete")) {
                    m_deleteCandidatePath = NormalizePath(path);
                    m_shouldOpenDeletePopup = true;
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Protected Asset Folder");
                ImGui::EndDisabled();
            }
            ImGui::EndPopup();
        }
        return;
    }

    const bool isModel = IsModelAsset(path);
    const bool isTexture = IsTextureAsset(path);
    std::string labelPrefix = "        ";
    if (isModel) {
        labelPrefix = "[MODEL] ";
    } else if (isTexture) {
        labelPrefix = "[TEX]   ";
    }
    const std::string label = labelPrefix + name;

    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (isModel && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            AddAssetToScene(scene, path);
        } else if (isTexture && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ApplyTextureToSelectedEntity(scene, path);
        }
    }

    if (isModel && ImGui::BeginDragDropSource()) {
        const std::string payloadPath = path.string();
        ImGui::SetDragDropPayload(kAssetPayloadType, payloadPath.c_str(), payloadPath.size() + 1);
        ImGui::Text("%s", name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginPopupContextItem()) {
        if (isModel && ImGui::MenuItem("Add to Scene")) {
            AddAssetToScene(scene, path);
        }
        if (isTexture && ImGui::MenuItem("Apply to Selected")) {
            ApplyTextureToSelectedEntity(scene, path);
        }
        if (CanModifyAssetPath(path)) {
            if (ImGui::MenuItem("Rename")) {
                BeginRenameAsset(path);
            }
            if (ImGui::MenuItem("Delete")) {
                m_deleteCandidatePath = NormalizePath(path);
                m_shouldOpenDeletePopup = true;
            }
        }
        ImGui::EndPopup();
    }
}

void EditorUI::DrawDeleteAssetPopup() {
    if (m_shouldOpenDeletePopup) {
        ImGui::OpenPopup("Delete Asset");
        m_shouldOpenDeletePopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::string displayPath = GetDisplayPath(m_deleteCandidatePath);
        ImGui::Text("Delete %s?", displayPath.c_str());

        std::error_code error;
        if (std::filesystem::is_directory(m_deleteCandidatePath, error)) {
            ImGui::TextDisabled("This will delete the folder and everything inside it.");
        }

        if (ImGui::Button("Delete")) {
            DeleteAsset(m_deleteCandidatePath);
            m_deleteCandidatePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_deleteCandidatePath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
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

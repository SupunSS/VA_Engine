#include "EditorUI.h"
#include <glad/glad.h>
#include "../core/Log.h"
#include "../rendering/Texture.h"
#include "../scene/Components.h"
#include "../scene/SceneLoader.h"
#include "../scene/SaveSystem.h"
#include "../physics/PhysicsWorld.h"
#include "../physics/CharacterController.h"
#include "../physics/VehicleController.h"
#include "../rendering/Primitives.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <limits>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../rendering/Material.h"
#include "../scripting/ScriptEngine.h"
#include <engine/HotReload.h>
#include <fstream>
#include <sstream>
#include "../core/AssetPaths.h"
#include "../rendering/Animator.h"
#include "../rendering/AnimationStateMachine.h"
#include "../rendering/AnimationStateMachineLoader.h"
#include "../rendering/Model.h"
#include <functional>
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

// Which shape the "Spawn Body" button in the Physics Test panel creates.
// Separate from PhysicsShapeType (the actual Jolt collision primitive)
// because there's no native pyramid collider yet — Pyramid uses a box
// collision shape sized to its bounding box under the hood while looking
// like a real pyramid visually. Sphere gets a real matching sphere collider.
enum class TestBodyVisualShape {
    Box,
    Sphere,
    Pyramid
};

entt::entity SpawnPhysicsTestBody(Scene& scene, PhysicsWorld& physicsWorld, const glm::vec3& position, bool isStatic,
                                   TestBodyVisualShape visualShape, float boxHalfExtent, float sphereRadius) {
    auto entity = scene.CreateEntity();
    auto& transform = scene.Registry.get<Transform>(entity);
    transform.Position = position;
    scene.Registry.emplace<PhysicsTestBody>(entity);

    if (visualShape == TestBodyVisualShape::Sphere) {
        // CreateSphere already bakes the radius into its generated
        // geometry, so Transform.Scale stays at 1 — scaling it further
        // would double-apply the radius.
        auto model = Primitives::CreateSphere(sphereRadius);
        scene.Registry.emplace<MeshRenderer>(entity, model);
        transform.Scale = glm::vec3(1.0f);

        const auto bodyId = physicsWorld.CreateSphereBody(position, sphereRadius, isStatic);
        scene.Registry.emplace<RigidBody>(entity, bodyId, isStatic, PhysicsShapeType::Sphere, glm::vec3(sphereRadius), sphereRadius);
        return entity;
    }

    if (visualShape == TestBodyVisualShape::Pyramid) {
        const float pyramidBaseHalfWidth = boxHalfExtent;
        const float pyramidHeight = boxHalfExtent * 2.0f;
        auto model = Primitives::CreatePyramid(pyramidBaseHalfWidth, pyramidHeight);
        scene.Registry.emplace<MeshRenderer>(entity, model);
        transform.Scale = glm::vec3(1.0f);

        // Box collider approximating the pyramid's bounding volume — see
        // the TestBodyVisualShape comment above for why.
        const glm::vec3 halfExtents(pyramidBaseHalfWidth, pyramidHeight * 0.5f, pyramidBaseHalfWidth);
        const auto bodyId = physicsWorld.CreateBoxBody(position, halfExtents, isStatic);
        scene.Registry.emplace<RigidBody>(entity, bodyId, isStatic, PhysicsShapeType::Box, halfExtents, 0.5f);
        return entity;
    }

    // Box (default)
    auto model = SceneLoader::GetOrLoadModel("models/cube.obj");
    scene.Registry.emplace<MeshRenderer>(entity, model);
    const glm::vec3 halfExtents(boxHalfExtent);
    transform.Scale = glm::vec3(halfExtents.x * 2.0f, halfExtents.y * 2.0f, halfExtents.z * 2.0f);
    const auto bodyId = physicsWorld.CreateBoxBody(position, halfExtents, isStatic);
    scene.Registry.emplace<RigidBody>(entity, bodyId, isStatic, PhysicsShapeType::Box, halfExtents, 0.5f);
    return entity;
}

void ResetPhysicsTestBodies(Scene& scene, PhysicsWorld& physicsWorld) {
    std::vector<entt::entity> toDestroy;
    auto view = scene.Registry.view<RigidBody, PhysicsTestBody>();
    for (auto entity : view) {
        auto& rigidBody = view.get<RigidBody>(entity);
        if (!rigidBody.BodyId.IsInvalid()) {
            physicsWorld.DestroyBody(rigidBody.BodyId);
        }
        toDestroy.push_back(entity);
    }

    for (auto entity : toDestroy) {
        scene.DestroyEntity(entity);
    }
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

// --- Ray picking helpers ----------------------------------------------------

glm::vec3 ComputeMouseRayDirection(const Camera& camera, float aspectRatio,
                                    double mouseX, double mouseY,
                                    int viewportWidth, int viewportHeight) {
    const float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(viewportWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(viewportHeight);

    const glm::mat4 proj = camera.GetProjectionMatrix(aspectRatio);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 invViewProj = glm::inverse(proj * view);

    glm::vec4 nearPoint = invViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    return glm::normalize(glm::vec3(farPoint - nearPoint));
}

bool RayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                        const glm::vec3& boxMin, const glm::vec3& boxMax, float& outDistance) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(rayDir[axis]) < 1e-8f) {
            if (rayOrigin[axis] < boxMin[axis] || rayOrigin[axis] > boxMax[axis]) {
                return false;
            }
            continue;
        }

        const float invDir = 1.0f / rayDir[axis];
        float t1 = (boxMin[axis] - rayOrigin[axis]) * invDir;
        float t2 = (boxMax[axis] - rayOrigin[axis]) * invDir;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return false;
        }
    }

    outDistance = tMin;
    return true;
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

    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    m_gpuVendor = vendor != nullptr ? vendor : "Unknown";
    m_gpuRenderer = renderer != nullptr ? renderer : "Unknown";
    m_glVersion = version != nullptr ? version : "Unknown";
    m_glslVersion = glslVersion != nullptr ? glslVersion : "Unknown";
    m_hardwareConcurrency = std::max(1u, std::thread::hardware_concurrency());
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
    ImGuizmo::BeginFrame();

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

void EditorUI::UpdatePerformanceStats(float deltaTime, int windowWidth, int windowHeight) {
    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;
    m_frameTimeMs = deltaTime * 1000.0f;
    m_fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
}

void EditorUI::ToggleStatsOverlay() {
    ShowStatsOverlay = !ShowStatsOverlay;
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
    if (!renderer.MaterialRef) {
    renderer.MaterialRef = std::make_shared<Material>();
    }
    renderer.MaterialRef->albedoMap = texture;

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
            ImGui::MenuItem("Script Editor", nullptr, &ShowScriptEditorPanel);
            ImGui::MenuItem("Inspector", nullptr, &ShowInspector);
            ImGui::MenuItem("Asset Browser", nullptr, &ShowAssetBrowser);
            ImGui::MenuItem("Physics Test", nullptr, &ShowPhysicsPanel);
            ImGui::MenuItem("Vehicle Test", nullptr, &ShowVehiclePanel);
            ImGui::MenuItem("Player", nullptr, &ShowPlayerPanel);
            ImGui::MenuItem("Viewport Settings", nullptr, &ShowViewportSettings);
            ImGui::MenuItem("Gizmo Toolbar", nullptr, &ShowGizmoToolbar);
            ImGui::MenuItem("Culling", nullptr, &ShowCullingPanel);
            ImGui::MenuItem("Culling Debug Overlay", nullptr, &ShowCullingDebugOverlay);
            ImGui::MenuItem("Save / Load", nullptr, &ShowSaveLoadPanel);
            ImGui::MenuItem("Animation", nullptr, &ShowAnimationPanel);
            ImGui::Separator();
            ImGui::MenuItem("Stats Overlay", "Alt+R", &ShowStatsOverlay);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Workspace")) {
            if (ImGui::MenuItem("Full (Engine Dev)", nullptr, m_currentWorkspace == Workspace::Full)) {
                ApplyWorkspace(Workspace::Full);
            }
            if (ImGui::MenuItem("Scripter", nullptr, m_currentWorkspace == Workspace::Scripter)) {
                ApplyWorkspace(Workspace::Scripter);
            }
            if (ImGui::MenuItem("Level Designer", nullptr, m_currentWorkspace == Workspace::LevelDesigner)) {
                ApplyWorkspace(Workspace::LevelDesigner);
            }
            if (ImGui::MenuItem("Animation", nullptr, m_currentWorkspace == Workspace::Animation)) {
                ApplyWorkspace(Workspace::Animation);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorUI::ApplyWorkspace(Workspace workspace) {
    m_currentWorkspace = workspace;

    switch (workspace) {
        case Workspace::Full:
            ShowSceneHierarchy = true;
            ShowInspector = true;
            ShowAssetBrowser = true;
            ShowPhysicsPanel = true;
            ShowVehiclePanel = true;
            ShowViewportSettings = true;
            ShowPlayerPanel = true;
            ShowGizmoToolbar = true;
            ShowCullingPanel = true;
            ShowSaveLoadPanel = true;
            ShowScriptEditorPanel = true;
            ShowCullingDebugOverlay = true;
            break;

        case Workspace::Scripter:
            ShowSceneHierarchy = true;   // needed to pick an entity to test against
            ShowInspector = true;        // see/tweak component values while testing a script
            ShowAssetBrowser = false;
            ShowPhysicsPanel = false;
            ShowVehiclePanel = false;
            ShowViewportSettings = false;
            ShowPlayerPanel = true;      // Play/Stop, to actually run the game and watch scripts fire
            ShowGizmoToolbar = false;
            ShowCullingPanel = false;
            ShowSaveLoadPanel = false;
            ShowScriptEditorPanel = true;
            ShowStatsOverlay = true;
            ShowCullingDebugOverlay = false;
            break;

        case Workspace::LevelDesigner:
            ShowSceneHierarchy = true;
            ShowInspector = true;
            ShowAssetBrowser = true;     // placing models/prefabs is the core of this job
            ShowPhysicsPanel = false;
            ShowVehiclePanel = false;
            ShowViewportSettings = true; // camera speed, grid toggle
            ShowPlayerPanel = true;      // test-drive the level
            ShowGizmoToolbar = true;     // move/rotate/scale placed objects
            ShowCullingPanel = true;     // tune render distance while building
            ShowSaveLoadPanel = true;
            ShowScriptEditorPanel = false;
            ShowCullingDebugOverlay = false;
            break;
        
        case Workspace::Animation:
            // Everything not needed to author/preview animations gets out
            // of the way — physics/vehicle/culling debug tools are noise
            // here, and the script editor competes for the same screen
            // space the state graph needs.
            ShowSceneHierarchy = true;    // pick which NPC/entity to inspect
            ShowInspector = true;         // confirm AnimatorComponent is present on selection
            ShowAssetBrowser = true;      // browse to .fbx clips / .json state machines
            ShowPhysicsPanel = false;
            ShowVehiclePanel = false;
            ShowViewportSettings = true;  // camera speed while lining up a preview shot
            ShowPlayerPanel = true;       // Play/Stop, to test transitions live
            ShowGizmoToolbar = false;
            ShowCullingPanel = false;
            ShowSaveLoadPanel = false;
            ShowScriptEditorPanel = false;
            ShowAnimationPanel = true;    // read-only debug view alongside the full editor
            ShowAnimatorEditor = true;    // the star of this workspace
            ShowCullingDebugOverlay = false;
            break;
    }
}

void EditorUI::DrawStatsOverlay() {
    if (!ShowStatsOverlay) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, 0.0f), ImVec2(320.0f, FLT_MAX));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##StatsOverlay", &ShowStatsOverlay, flags)) {
        ImGui::Text("Performance Stats");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", m_fps);
        ImGui::Text("Frame: %.2f ms", m_frameTimeMs);
        ImGui::Text("Resolution: %dx%d", m_windowWidth, m_windowHeight);
        ImGui::Separator();
        ImGui::Text("GPU: %s", m_gpuVendor.c_str());
        ImGui::TextWrapped("Renderer: %s", m_gpuRenderer.c_str());
        ImGui::Text("OpenGL: %s", m_glVersion.c_str());
        ImGui::Text("GLSL: %s", m_glslVersion.c_str());
        ImGui::Text("Threads: %u", m_hardwareConcurrency);
    }
    ImGui::End();
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

void EditorUI::DrawInspector(Scene& scene, ScriptEngine& scriptEngine) {
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

    if (renderer.MaterialRef) {
        ImGui::Text("Albedo: %s", renderer.MaterialRef->albedoMap ? "Custom" : "None");
        ImGui::Text("Normal Map: %s", renderer.MaterialRef->normalMap ? "Assigned" : "None");
        ImGui::ColorEdit3("Tint", &renderer.MaterialRef->albedoTint.x);
        ImGui::SliderFloat("Roughness", &renderer.MaterialRef->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &renderer.MaterialRef->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Normal Strength", &renderer.MaterialRef->normalStrength, 0.0f, 2.0f);
    } else {
        ImGui::Text("Material: None");
    }
}

        // --- Script attachment ------------------------------------------
        ImGui::Separator();
        ImGui::Text("Script");

        if (!m_scriptFilesLoaded) {
            RefreshScriptFileList();
        }

        const bool hasScript = scriptEngine.HasScript(SelectedEntity);
        if (hasScript) {
            std::string attachedPath = scriptEngine.GetAttachedScriptPath(SelectedEntity);
            std::filesystem::path attachedFilename = std::filesystem::path(attachedPath).filename();
            ImGui::Text("Attached: %s", attachedFilename.string().c_str());

            if (ImGui::Button("Reload##EntityScript")) {
                scriptEngine.AttachScript(SelectedEntity, attachedPath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##EntityScript")) {
                scriptEngine.DetachScript(SelectedEntity);
            }
        } else {
            ImGui::TextDisabled("No script attached");

            if (m_scriptFiles.empty()) {
                ImGui::TextDisabled("(no .lua files found — check the Script Editor panel)");
            } else {
                static int selectedScriptIndex = 0;
                if (selectedScriptIndex >= static_cast<int>(m_scriptFiles.size())) {
                    selectedScriptIndex = 0;
                }

                std::vector<std::string> names;
                names.reserve(m_scriptFiles.size());
                for (const auto& p : m_scriptFiles) {
                    names.push_back(p.filename().string());
                }

                if (ImGui::BeginCombo("##AttachScriptCombo", names[selectedScriptIndex].c_str())) {
                    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                        const bool isSelected = (i == selectedScriptIndex);
                        if (ImGui::Selectable(names[i].c_str(), isSelected)) {
                            selectedScriptIndex = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Attach##EntityScript")) {
                    scriptEngine.AttachScript(SelectedEntity, m_scriptFiles[selectedScriptIndex].string());
                }
            }
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

void EditorUI::DrawPhysicsPanel(Scene& scene, PhysicsWorld& physicsWorld) {
    if (!ShowPhysicsPanel) return;
    ImGui::Begin("Physics Test", &ShowPhysicsPanel);

    static bool physicsEnabled = true;
    static float gravity = -9.81f;
    static float spawnHeight = 4.0f;
    static bool spawnStatic = false;
    static int currentShape = static_cast<int>(TestBodyVisualShape::Box);
    static float boxHalfExtent = 0.5f;
    static float sphereRadius = 0.5f;

    if (ImGui::Checkbox("Enabled", &physicsEnabled)) {
        physicsWorld.SetEnabled(physicsEnabled);
    }

    if (ImGui::SliderFloat("Gravity", &gravity, -20.0f, 20.0f, "%.2f")) {
        physicsWorld.SetGravity(glm::vec3(0.0f, gravity, 0.0f));
    }

    ImGui::Separator();
    ImGui::Combo("Shape", &currentShape, "Box\0Sphere\0Pyramid\0");

    const auto visualShape = static_cast<TestBodyVisualShape>(currentShape);
    if (visualShape == TestBodyVisualShape::Sphere) {
        ImGui::SliderFloat("Radius", &sphereRadius, 0.1f, 2.0f, "%.2f");
    } else if (visualShape == TestBodyVisualShape::Pyramid) {
        ImGui::SliderFloat("Base Half Width", &boxHalfExtent, 0.1f, 2.0f, "%.2f");
    } else {
        ImGui::SliderFloat("Half Extent", &boxHalfExtent, 0.1f, 2.0f, "%.2f");
    }

    ImGui::SliderFloat("Spawn Height", &spawnHeight, 1.0f, 10.0f, "%.1f");
    ImGui::Checkbox("Static Body", &spawnStatic);

    if (ImGui::Button("Spawn Body")) {
        const auto entity = SpawnPhysicsTestBody(
            scene,
            physicsWorld,
            glm::vec3(0.0f, spawnHeight, 0.0f),
            spawnStatic,
            visualShape,
            boxHalfExtent,
            sphereRadius
        );
        SelectedEntity = entity;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Bodies")) {
        ResetPhysicsTestBodies(scene, physicsWorld);
    }

    ImGui::Separator();
    const int physicsBodyCount = static_cast<int>(scene.Registry.view<RigidBody>().size());
    ImGui::Text("Physics bodies: %d", physicsBodyCount);
    ImGui::Text("Simulation: %s", physicsEnabled ? "Running" : "Paused");

    if (SelectedEntity != entt::null && scene.Registry.valid(SelectedEntity) && scene.Registry.all_of<RigidBody>(SelectedEntity)) {
        const auto& rigidBody = scene.Registry.get<RigidBody>(SelectedEntity);
        ImGui::Separator();
        ImGui::Text("Selected body");
        ImGui::Text("Shape: %s", rigidBody.Shape == PhysicsShapeType::Box ? "Box" : "Sphere");
        ImGui::Text("Static: %s", rigidBody.IsStatic ? "Yes" : "No");
        if (!rigidBody.BodyId.IsInvalid()) {
            const auto bodyPosition = physicsWorld.GetBodyPosition(rigidBody.BodyId);
            ImGui::Text("Position: %.2f, %.2f, %.2f", bodyPosition.x, bodyPosition.y, bodyPosition.z);
        }
    }

    ImGui::End();
}

bool EditorUI::DrawVehiclePanel(Scene& scene, PhysicsWorld& physicsWorld, VehicleController* activeVehicle,
                                 const glm::vec3& spawnPos, bool& outDespawnRequested) {
    outDespawnRequested = false;
    if (!ShowVehiclePanel) return false;
    ImGui::Begin("Vehicle Test", &ShowVehiclePanel);

    bool spawnRequested = false;
    if (ImGui::Button("Spawn Vehicle")) {
        spawnRequested = true;
    }
    ImGui::TextDisabled("Spawns just beside your current position");

    ImGui::SameLine();
    if (activeVehicle == nullptr) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Despawn Vehicle")) {
        outDespawnRequested = true;
    }
    if (activeVehicle == nullptr) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    if (activeVehicle != nullptr) {
        ImGui::Text("Speed: %.1f km/h", activeVehicle->GetSpeedKmh());
        ImGui::Text("Engine RPM: %.0f", activeVehicle->GetRPM());
        ImGui::Text("Gear: %d", activeVehicle->GetTransmissionGear());

        ImGui::Separator();
        ImGui::Text("Tuning Parameters (Live)");

        float torque = activeVehicle->GetEngineTorque();
        if (ImGui::SliderFloat("Engine Torque", &torque, 100.0f, 2000.0f, "%.0f Nm")) {
            activeVehicle->SetEngineTorque(torque);
        }

        float freq = activeVehicle->GetSuspensionFrequency();
        float damp = activeVehicle->GetSuspensionDamping();
        if (ImGui::SliderFloat("Suspension Frequency", &freq, 0.5f, 5.0f, "%.2f Hz")) {
            activeVehicle->SetSuspensionParameters(freq, damp);
        }
        if (ImGui::SliderFloat("Suspension Damping", &damp, 0.1f, 2.0f, "%.2f")) {
            activeVehicle->SetSuspensionParameters(freq, damp);
        }

        float friction = activeVehicle->GetTireFriction();
        if (ImGui::SliderFloat("Tire Friction", &friction, 0.5f, 4.0f, "%.2f")) {
            activeVehicle->SetTireFriction(friction);
        }

        float steer = activeVehicle->GetMaxSteerAngleDegrees();
        if (ImGui::SliderFloat("Max Steer Angle", &steer, 15.0f, 50.0f, "%.1f deg")) {
            activeVehicle->SetMaxSteerAngleDegrees(steer);
        }
    } else {
        ImGui::TextDisabled("No vehicle active. Enter play mode or spawn vehicle.");
    }

    ImGui::Separator();
    ImGui::Text("Vehicle Controls:");
    ImGui::TextWrapped("Walk up to vehicle and press 'F' to enter/exit. Accelerate/Reverse: W/S, Steer: A/D, Handbrake: Space.");

    ImGui::End();
    return spawnRequested;
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
    ImGui::Separator();
    if (ImGui::Button(ShowStatsOverlay ? "Hide Stats Overlay" : "Show Stats Overlay")) {
        ToggleStatsOverlay();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Alt+R");

    ImGui::End();
}

void EditorUI::DrawPlayerPanel(bool& playMode, CharacterController* controller) {
    if (!ShowPlayerPanel) return;
    ImGui::Begin("Player", &ShowPlayerPanel);

    if (ImGui::Button(playMode ? "Stop" : "Play")) {
        playMode = !playMode;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Esc also exits Play mode");

    if (controller) {
        ImGui::SliderFloat("Walk Speed", &controller->WalkSpeed, 1.0f, 10.0f);
        ImGui::SliderFloat("Sprint Speed", &controller->SprintSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Jump Speed", &controller->JumpSpeed, 1.0f, 12.0f);
        ImGui::Text(controller->IsGrounded() ? "Grounded" : "Airborne");
    } else {
        ImGui::TextDisabled("No character controller bound");
    }

    ImGui::End();
}

void EditorUI::DrawCullingPanel(bool& freezeCullingFrustum, float& maxRenderDistance, int renderedCount, int culledCount) {
    if (!ShowCullingPanel) return;
    ImGui::Begin("Culling", &ShowCullingPanel);

    ImGui::Checkbox("Freeze Culling Frustum", &freezeCullingFrustum);
    ImGui::TextWrapped("While frozen, use Left/Right/Up/Down arrow keys to pan/tilt "
                        "the yellow wireframe frustum in place. Fly the free-fly "
                        "camera around normally to watch objects render/cull live "
                        "as you sweep it.");

    ImGui::Separator();
    ImGui::SliderFloat("Max Render Distance", &maxRenderDistance, 20.0f, 1000.0f, "%.0f");

    ImGui::Separator();
    ImGui::Text("Rendered: %d", renderedCount);
    ImGui::Text("Culled: %d", culledCount);
    const int total = renderedCount + culledCount;
    if (total > 0) {
        ImGui::Text("Culled: %.1f%%", 100.0f * static_cast<float>(culledCount) / static_cast<float>(total));
    }

    ImGui::End();
}

void EditorUI::DrawCullingDebugOverlay(float cameraYaw, float cameraPitch, float cameraDepthToVehicle,
                                        float vehicleDistance, float vehicleRadius, bool vehicleWithinDistance,
                                        bool vehicleInsideFrustum, int visibleWheelCount) {
    if (!ShowCullingDebugOverlay) return;

    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowPos(ImVec2(20.0f, 400.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Culling Debug", &ShowCullingDebugOverlay, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Camera Yaw: %.1f  Pitch: %.1f", cameraYaw, cameraPitch);
    ImGui::Separator();
    ImGui::Text("Vehicle distance: %.2f  radius: %.2f", vehicleDistance, vehicleRadius);
    ImGui::Text("Depth along view: %.2f", cameraDepthToVehicle);
    ImGui::TextColored(vehicleWithinDistance ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                        "withinDistance: %s", vehicleWithinDistance ? "true" : "FALSE");
    ImGui::TextColored(vehicleInsideFrustum ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                        "insideFrustum: %s", vehicleInsideFrustum ? "true" : "FALSE");
    ImGui::Text("Visible wheels: %d / 4", visibleWheelCount);
    ImGui::End();
}
// --- Gizmo / selection -------------------------------------------------

void EditorUI::DrawGizmoToolbar() {
    if (!ShowGizmoToolbar) return;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Gizmo Toolbar", &ShowGizmoToolbar,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

    auto modeButton = [this](const char* label, GizmoOperation mode) {
        const bool active = CurrentGizmoOperation == mode;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        }
        if (ImGui::Button(label)) {
            CurrentGizmoOperation = mode;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    };

    modeButton("Move (W)", GizmoOperation::Translate);
    modeButton("Rotate (E)", GizmoOperation::Rotate);
    modeButton("Scale (R)", GizmoOperation::Scale);

    ImGui::NewLine();
    ImGui::TextDisabled("Click to select, Delete to remove");

    ImGui::End();
}

bool EditorUI::IsGizmoActive() const {
    return ImGuizmo::IsUsing();
}

void EditorUI::DrawTransformGizmo(Scene& scene, PhysicsWorld& physicsWorld, const Camera& camera, float aspectRatio) {
    if (SelectedEntity == entt::null || !scene.Registry.valid(SelectedEntity)) {
        return;
    }
    if (!scene.Registry.all_of<Transform>(SelectedEntity)) {
        return;
    }

    // ImGuizmo::SetDrawlist() attaches to whatever ImGui window is currently
    // open — it has no valid target if called outside a Begin/End pair, so we
    // open an invisible fullscreen overlay window purely to host the gizmo's
    // draw calls and mouse hit-testing.
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_windowWidth), static_cast<float>(m_windowHeight)));
    ImGui::Begin("##GizmoOverlay", nullptr,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(m_windowWidth), static_cast<float>(m_windowHeight));

    auto& transform = scene.Registry.get<Transform>(SelectedEntity);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.Position)
                     * glm::mat4_cast(transform.Rotation)
                     * glm::scale(glm::mat4(1.0f), transform.Scale);

    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 proj = camera.GetProjectionMatrix(aspectRatio);

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    if (CurrentGizmoOperation == GizmoOperation::Rotate) {
        operation = ImGuizmo::ROTATE;
    } else if (CurrentGizmoOperation == GizmoOperation::Scale) {
        operation = ImGuizmo::SCALE;
    }

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        operation,
        ImGuizmo::WORLD,
        glm::value_ptr(model)
    );

    if (ImGuizmo::IsUsing()) {
        float translationValues[3];
        float rotationValues[3];
        float scaleValues[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), translationValues, rotationValues, scaleValues);

        transform.Position = glm::vec3(translationValues[0], translationValues[1], translationValues[2]);
        transform.Rotation = glm::quat(glm::radians(glm::vec3(rotationValues[0], rotationValues[1], rotationValues[2])));
        transform.Scale = glm::vec3(scaleValues[0], scaleValues[1], scaleValues[2]);

        // Teleport the physics body to match, so main.cpp's per-frame sync
        // (transform.Position = physicsWorld.GetBodyPosition(...)) doesn't
        // stomp this edit right back on the next physics step.
        //
        // NOTE: this only syncs Position/Rotation, not Scale. Jolt collision
        // shapes aren't trivially resizable at runtime through
        // BodyInterface — that needs recreating the shape, which isn't
        // wired up here. So scaling a RigidBody entity with the gizmo will
        // resize it visually but its collider stays its original size until
        // that's added.
        if (scene.Registry.all_of<RigidBody>(SelectedEntity)) {
            auto& rigidBody = scene.Registry.get<RigidBody>(SelectedEntity);
            if (!rigidBody.BodyId.IsInvalid()) {
                const JPH::RVec3 physicsPosition(transform.Position.x, transform.Position.y, transform.Position.z);
                const JPH::Quat physicsRotation(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                physicsWorld.GetBodyInterface().SetPositionAndRotation(
                    rigidBody.BodyId, physicsPosition, physicsRotation, JPH::EActivation::Activate);
            }
        }

        // Vehicles use VehicleComponent (a chassis body owned internally by
        // VehicleController), not RigidBody — so the block above never
        // fires for them. Without this, main.cpp's per-frame chassis sync
        // (transform.Position = vehicleComp.Controller->GetChassisTransform(...))
        // would immediately snap the vehicle right back to wherever Jolt's
        // chassis body actually is, making the gizmo look like it does
        // nothing — it moves the Transform for one frame, then the very
        // next frame's vehicle sync overwrites it right back.
        if (scene.Registry.all_of<VehicleComponent>(SelectedEntity)) {
            auto& vehicleComp = scene.Registry.get<VehicleComponent>(SelectedEntity);
            if (vehicleComp.Controller) {
                const JPH::BodyID chassisBodyId = vehicleComp.Controller->GetBodyID();
                if (!chassisBodyId.IsInvalid()) {
                    const JPH::RVec3 physicsPosition(transform.Position.x, transform.Position.y, transform.Position.z);
                    const JPH::Quat physicsRotation(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                    physicsWorld.GetBodyInterface().SetPositionAndRotation(
                        chassisBodyId, physicsPosition, physicsRotation, JPH::EActivation::Activate);
                }
            }
        }
    }

    ImGui::End();
}

void EditorUI::HandleViewportClick(Scene& scene, const Camera& camera, float aspectRatio,
                                    double mouseX, double mouseY, int viewportWidth, int viewportHeight) {
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    const glm::vec3 rayOrigin = camera.Position;
    const glm::vec3 rayDir = ComputeMouseRayDirection(camera, aspectRatio, mouseX, mouseY, viewportWidth, viewportHeight);

    entt::entity closestEntity = entt::null;
    float closestDistance = std::numeric_limits<float>::max();

    auto view = scene.Registry.view<Transform, MeshRenderer>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& renderer = view.get<MeshRenderer>(entity);

        if (!renderer.ModelRef) {
            continue;
        }

        // Use the model's real local-space bounds instead of assuming
        // every mesh is a unit cube. This is what makes newly-added
        // primitives (spheres, pyramids) and any imported .fbx/.gltf
        // model get an accurately-sized click hitbox that scales with
        // their actual geometry, rather than a fixed 0.5-unit box that
        // was only ever correct for cube.obj.
        const glm::vec3 boundsMin = renderer.ModelRef->GetBoundsMin();
        const glm::vec3 boundsMax = renderer.ModelRef->GetBoundsMax();

        const std::array<glm::vec3, 8> localCorners = {
            glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z),
            glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z),
            glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z),
            glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z),
            glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z),
            glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z),
            glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z),
            glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z),
        };

        const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.Position)
                               * glm::mat4_cast(transform.Rotation)
                               * glm::scale(glm::mat4(1.0f), transform.Scale);

        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(std::numeric_limits<float>::lowest());
        for (const auto& corner : localCorners) {
            const glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }

        float distance = 0.0f;
        if (RayIntersectsAABB(rayOrigin, rayDir, worldMin, worldMax, distance) && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    }

    if (closestEntity != entt::null) {
        SelectedEntity = closestEntity;
    }
}

void EditorUI::DeleteSelectedEntity(Scene& scene, PhysicsWorld& physicsWorld) {
    if (SelectedEntity == entt::null || !scene.Registry.valid(SelectedEntity)) {
        return;
    }

    // Record the deletion as a persistent chunk delta BEFORE any teardown —
    // this is what makes destroyed buildings/entities stay destroyed after
    // the chunk streams out and back in, or after a save/load. No-ops
    // safely for entities that aren't chunk-tagged (e.g. physics test props).
    SceneLoader::RecordEntityDestructionDelta(scene, SelectedEntity);

    if (scene.Registry.all_of<RigidBody>(SelectedEntity)) {
        auto& rigidBody = scene.Registry.get<RigidBody>(SelectedEntity);
        if (!rigidBody.BodyId.IsInvalid()) {
            physicsWorld.DestroyBody(rigidBody.BodyId);
        }
    }

    scene.DestroyEntity(SelectedEntity);
    SelectedEntity = entt::null;
}

// --- Save / Load ---------------------------------------------------------

bool EditorUI::DrawSaveLoadPanel(std::string& outSlotName, bool& outIsSaveAction) {
    if (!ShowSaveLoadPanel) return false;

    bool actionRequested = false;
    ImGui::Begin("Save / Load", &ShowSaveLoadPanel);

    if (!m_saveSlotsLoaded) {
        m_cachedSaveSlots = SaveSystem::ListSaveSlots();
        m_saveSlotsLoaded = true;
    }

    ImGui::TextUnformatted("Slot name:");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##SaveSlotName", m_saveSlotNameBuffer, sizeof(m_saveSlotNameBuffer));

    const bool hasSlotName = m_saveSlotNameBuffer[0] != '\0';
    if (!hasSlotName) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save Game", ImVec2(120.0f, 0.0f))) {
        outSlotName = m_saveSlotNameBuffer;
        outIsSaveAction = true;
        actionRequested = true;
        m_saveSlotsLoaded = false; // force a refresh next frame so the new slot appears in the list
    }
    if (!hasSlotName) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Existing saves:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        m_cachedSaveSlots = SaveSystem::ListSaveSlots();
        m_saveSlotsLoaded = true;
    }

    if (m_cachedSaveSlots.empty()) {
        ImGui::TextDisabled("No saves yet.");
    } else {
        // Two-pass: draw the list first, then apply any delete requested
        // during the pass. Deleting mid-iteration would invalidate the
        // range-for over m_cachedSaveSlots while we're still using it.
        int deleteIndex = -1;

        for (int i = 0; i < static_cast<int>(m_cachedSaveSlots.size()); ++i) {
            const std::string& slot = m_cachedSaveSlots[i];
            ImGui::PushID(slot.c_str());
            ImGui::BulletText("%s", slot.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Load")) {
                outSlotName = slot;
                outIsSaveAction = false;
                actionRequested = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                m_deleteSaveCandidate = slot;
                m_shouldOpenDeleteSavePopup = true;
            }
            ImGui::PopID();
        }
    }

    // Confirmation popup — deleting a save is destructive and not undoable,
    // so this follows the same pattern as DrawDeleteAssetPopup rather than
    // deleting instantly on click.
    if (m_shouldOpenDeleteSavePopup) {
        ImGui::OpenPopup("Delete Save");
        m_shouldOpenDeleteSavePopup = false;
    }

    if (ImGui::BeginPopupModal("Delete Save", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete save slot '%s'? This cannot be undone.", m_deleteSaveCandidate.c_str());

        if (ImGui::Button("Delete")) {
            SaveSystem::DeleteSaveSlot(m_deleteSaveCandidate);
            m_deleteSaveCandidate.clear();
            m_saveSlotsLoaded = false; // force a refresh so the deleted slot disappears from the list
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_deleteSaveCandidate.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
    return actionRequested;
}

namespace {
int ScriptEditorResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}
} // namespace

void EditorUI::RefreshScriptFileList() {
    m_scriptFiles.clear();

    std::filesystem::path scriptsDir = m_projectRoot.empty()
        ? std::filesystem::path(AssetPaths::Root(AssetPaths::Category::Scripts))
        : m_projectRoot / AssetPaths::Root(AssetPaths::Category::Scripts);

    if (!std::filesystem::exists(scriptsDir)) {
        std::filesystem::create_directories(scriptsDir);
    }

    for (const auto& entry : std::filesystem::directory_iterator(scriptsDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            m_scriptFiles.push_back(entry.path());
        }
    }

    m_scriptFilesLoaded = true;
}

void EditorUI::LoadScriptIntoEditor(const std::filesystem::path& scriptPath) {
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
        m_scriptStatusMessage = "Failed to open " + scriptPath.string();
        return;
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    m_selectedScriptPath = scriptPath;
    m_scriptEditBuffer = contents.str();
    m_scriptBufferDirty = false;
    m_scriptStatusMessage = "Loaded " + scriptPath.filename().string();
}

void EditorUI::SaveCurrentScript(ScriptEngine& scriptEngine) {
    if (m_selectedScriptPath.empty()) {
        m_scriptStatusMessage = "No script selected";
        return;
    }

    std::ofstream file(m_selectedScriptPath, std::ios::trunc);
    if (!file.is_open()) {
        m_scriptStatusMessage = "Failed to save " + m_selectedScriptPath.string();
        return;
    }
    file << m_scriptEditBuffer;
    file.close();

    m_scriptBufferDirty = false;

    const std::string pathStr = m_selectedScriptPath.string();

    // Run it immediately — ScriptEngine::RunScript sets its own internal
    // m_currentScriptPath/m_lastWriteTime, which its existing
    // CheckForReload(float) already polls every frame from the main loop.
    scriptEngine.RunScript(pathStr);

    // Also register with the generic HotReloadManager exactly once per
    // path, so external edits to this same file (outside this panel)
    // trigger a re-run too, instead of only in-editor saves doing so.
    if (!m_watchedScriptPaths.contains(pathStr)) {
        GetHotReloadManager().Watch(pathStr, [&scriptEngine, pathStr](const std::string&) {
            scriptEngine.RunScript(pathStr);
        });
        m_watchedScriptPaths.insert(pathStr);
    }

    m_scriptStatusMessage = "Saved & ran " + m_selectedScriptPath.filename().string();
}

void EditorUI::DrawScriptEditorPanel(ScriptEngine& scriptEngine) {
    if (!ShowScriptEditorPanel) return;

    if (!m_scriptFilesLoaded) {
        RefreshScriptFileList();
    }

    ImGui::Begin("Script Editor", &ShowScriptEditorPanel);

    ImGui::BeginChild("ScriptFileList", ImVec2(180.0f, 0.0f), true);
    if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
        RefreshScriptFileList();
    }
    ImGui::Separator();
    for (const auto& scriptPath : m_scriptFiles) {
        bool isSelected = (scriptPath == m_selectedScriptPath);
        std::string label = scriptPath.filename().string();
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            if (m_scriptBufferDirty) {
                m_scriptStatusMessage = "Unsaved changes — save or discard before switching files";
            } else {
                LoadScriptIntoEditor(scriptPath);
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (m_selectedScriptPath.empty()) {
        ImGui::TextUnformatted("Select a script on the left to begin editing.");
    } else {
        ImGui::Text("Editing: %s%s", m_selectedScriptPath.filename().string().c_str(),
                    m_scriptBufferDirty ? " *" : "");

        if (ImGui::Button("Save & Run")) {
            SaveCurrentScript(scriptEngine);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard Changes")) {
            LoadScriptIntoEditor(m_selectedScriptPath);
        }

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize;
        ImVec2 editorSize = ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - 24.0f);

        if (ImGui::InputTextMultiline("##ScriptEditorBuffer", m_scriptEditBuffer.data(),
                                       m_scriptEditBuffer.capacity() + 1, editorSize, flags,
                                       ScriptEditorResizeCallback, &m_scriptEditBuffer)) {
            m_scriptBufferDirty = true;
        }
    }
    ImGui::EndGroup();

    if (!m_scriptStatusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(m_scriptStatusMessage.c_str());
    }

    ImGui::End();
}

void EditorUI::DrawAnimationPanel(Scene& scene, AnimationStateMachine* playerStateMachine, Animator* playerAnimator) {
    if (!ShowAnimationPanel) return;
    ImGui::Begin("Animation", &ShowAnimationPanel);

    auto drawMachineDebug = [](const char* label, AnimationStateMachine* machine, Animator* animator) {
        ImGui::SeparatorText(label);
        if (!machine || !animator) {
            ImGui::TextDisabled("Not available");
            return;
        }

        ImGui::Text("Current State: %s", machine->GetCurrentState().c_str());

        if (animator->IsBlending()) {
            const float duration = animator->GetBlendDuration();
            const float progress = duration > 0.0f
                ? std::clamp(animator->GetBlendElapsed() / duration, 0.0f, 1.0f)
                : 1.0f;
            ImGui::ProgressBar(progress, ImVec2(-1, 0), "Blending");
        } else {
            ImGui::TextDisabled("Not blending");
        }

        if (ImGui::TreeNode((std::string("Parameters##") + label).c_str())) {
            for (const auto& [name, value] : machine->GetBoolParams()) {
                ImGui::Text("%s (bool): %s", name.c_str(), value ? "true" : "false");
            }
            for (const auto& [name, value] : machine->GetFloatParams()) {
                ImGui::Text("%s (float): %.2f", name.c_str(), value);
            }
            for (const auto& [name, value] : machine->GetIntParams()) {
                ImGui::Text("%s (int): %d", name.c_str(), value);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode((std::string("States##") + label).c_str())) {
            for (const auto& [name, stateDef] : machine->GetStates()) {
                ImGui::BulletText("%s (blend in %.2fs)", name.c_str(), stateDef.BlendInDuration);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode((std::string("Transitions##") + label).c_str())) {
            for (const auto& transition : machine->GetTransitions()) {
                ImGui::BulletText("%s -> %s (%d condition%s)",
                    transition.FromState.c_str(), transition.ToState.c_str(),
                    static_cast<int>(transition.Conditions.size()),
                    transition.Conditions.size() == 1 ? "" : "s");
            }
            ImGui::TreePop();
        }
    };

    drawMachineDebug("Player", playerStateMachine, playerAnimator);

    ImGui::Separator();

    if (SelectedEntity != entt::null && scene.Registry.valid(SelectedEntity) &&
        scene.Registry.all_of<AnimatorComponent>(SelectedEntity)) {
        auto& animComp = scene.Registry.get<AnimatorComponent>(SelectedEntity);
        drawMachineDebug("Selected Entity", animComp.StateMachine.get(), animComp.AnimatorPtr.get());
    } else {
        ImGui::SeparatorText("Selected Entity");
        ImGui::TextDisabled("Select an entity with an AnimatorComponent to inspect it");
    }

    ImGui::End();
}

void EditorUI::DrawAnimatorEditorPanel(const std::vector<AnimatorEditTarget>& targets) {
    if (!ShowAnimatorEditor) return;
    ImGui::Begin("Animator Editor", &ShowAnimatorEditor);

    if (targets.empty()) {
        ImGui::TextDisabled("No animatable target available. Enter Play mode for the player, "
                             "or select a scene entity with an AnimatorComponent.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("AnimatorEditorTargets")) {
        for (const auto& target : targets) {
            if (!target.Machine || !target.AnimatorPtr) continue;
            if (ImGui::BeginTabItem(target.DisplayName.c_str())) {
                DrawAnimatorTargetEditor(target);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorUI::DrawAnimatorTargetEditor(const AnimatorEditTarget& target) {
    AnimatorEditorTargetState& state = m_animatorEditorState[target.Key];
    AnimationStateMachine& machine = *target.Machine;

    const bool canSave = !machine.GetSourceFilePath().empty();
    if (!canSave) ImGui::BeginDisabled();
    if (ImGui::Button("Save")) {
        AnimationStateMachineLoader::StateMachineLayout layout;
        for (const auto& [name, pos] : state.NodePositions) {
            layout.NodePositions[name] = { pos.x, pos.y };
        }
        state.StatusMessage = AnimationStateMachineLoader::SaveToFile(machine, machine.GetSourceFilePath(), &layout)
            ? "Saved." : "Save failed - see log.";
    }
    if (!canSave) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Save As...")) {
        std::strncpy(state.SaveAsPathBuffer, machine.GetSourceFilePath().c_str(), sizeof(state.SaveAsPathBuffer) - 1);
        ImGui::OpenPopup("SaveAnimatorAsPopup");
    }

    if (ImGui::BeginPopup("SaveAnimatorAsPopup")) {
        ImGui::InputText("Path", state.SaveAsPathBuffer, sizeof(state.SaveAsPathBuffer));
        ImGui::TextDisabled("Path under the anim state machines folder, e.g. characters/player.animsm.json");
        if (ImGui::Button("Save")) {
            AnimationStateMachineLoader::StateMachineLayout layout;
            for (const auto& [name, pos] : state.NodePositions) {
                layout.NodePositions[name] = { pos.x, pos.y };
            }
            const std::string resolvedPath = AssetPaths::Resolve(AssetPaths::Category::AnimStateMachines, state.SaveAsPathBuffer);
            if (AnimationStateMachineLoader::SaveToFile(machine, resolvedPath, &layout)) {
                machine.SetSourceFilePath(resolvedPath);
                state.StatusMessage = "Saved to " + resolvedPath;
            } else {
                state.StatusMessage = "Save failed - see log.";
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Preview Mode", &state.PreviewMode);
    if (state.PreviewMode) {
        target.AnimatorPtr->BeginPreview();
        ImGui::SameLine();
        ImGui::TextDisabled("(playback frozen - scrub the timeline below)");
    } else {
        target.AnimatorPtr->EndPreview();
    }

    if (!state.StatusMessage.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", state.StatusMessage.c_str());
    }

    ImGui::Separator();

    if (machine.GetStates().size() >= 2) {
        if (ImGui::Button("+ Add Transition")) {
            auto it = machine.GetStates().begin();
            std::string from = it->first;
            ++it;
            std::string to = (it != machine.GetStates().end()) ? it->first : from;
            machine.AddTransition(from, to, {});
            state.SelectedTransitionIndex = static_cast<int>(machine.GetTransitions().size()) - 1;
            state.SelectedState.clear();
        }
        ImGui::SameLine();
    }
    ImGui::TextDisabled("Right-click canvas: add state. Drag nodes to arrange. Click an arrow to edit it.");

    DrawStateGraphCanvas(target, state);

    ImGui::Separator();

    if (!state.SelectedState.empty() && machine.GetStates().count(state.SelectedState)) {
        DrawStateInspector(target, state);
    } else if (state.SelectedTransitionIndex >= 0 &&
               state.SelectedTransitionIndex < static_cast<int>(machine.GetTransitions().size())) {
        DrawTransitionInspector(target, state);
    } else {
        ImGui::TextDisabled("Select a state or transition above to edit it.");
    }

    ImGui::Separator();

    ImGui::Columns(2, nullptr, true);
    DrawBoneTree(target, state);
    ImGui::NextColumn();
    DrawAnimatorTimeline(target, state);
    ImGui::Columns(1);
}

void EditorUI::DrawStateGraphCanvas(const AnimatorEditTarget& target, AnimatorEditorTargetState& state) {
    AnimationStateMachine& machine = *target.Machine;

    ImGui::BeginChild("GraphCanvas", ImVec2(0, 300), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    drawList->AddRectFilled(canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                             IM_COL32(28, 28, 32, 255));

    int autoIndex = 0;
    for (const auto& [name, def] : machine.GetStates()) {
        if (state.NodePositions.find(name) == state.NodePositions.end()) {
            const float col = static_cast<float>(autoIndex % 4);
            const float row = static_cast<float>(autoIndex / 4);
            state.NodePositions[name] = glm::vec2(40.0f + col * 190.0f, 30.0f + row * 100.0f);
        }
        ++autoIndex;
    }

    const ImVec2 nodeSize(160.0f, 60.0f);
    auto nodeTopLeft = [&](const std::string& name) {
        const glm::vec2 p = state.NodePositions[name] + state.PanOffset;
        return ImVec2(canvasOrigin.x + p.x, canvasOrigin.y + p.y);
    };

    // Background catch-all first, so nodes/transitions added after it take
    // input priority wherever they overlap.
    ImGui::SetCursorScreenPos(canvasOrigin);
    ImGui::InvisibleButton("GraphCanvasBackground", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        state.PanOffset.x += delta.x;
        state.PanOffset.y += delta.y;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        state.PendingNewStatePos = glm::vec2(mouse.x - canvasOrigin.x, mouse.y - canvasOrigin.y) - state.PanOffset;
        state.NewStateName[0] = '\0';
        state.NewStateClipPath[0] = '\0';
        state.NewStateBlendIn = 0.2f;
        ImGui::OpenPopup("AddAnimatorStatePopup");
    }

    if (ImGui::BeginPopup("AddAnimatorStatePopup")) {
        ImGui::TextUnformatted("New State");
        ImGui::InputText("Name", state.NewStateName, sizeof(state.NewStateName));
        ImGui::InputText("Clip Path", state.NewStateClipPath, sizeof(state.NewStateClipPath));
        ImGui::SliderFloat("Blend In", &state.NewStateBlendIn, 0.0f, 2.0f, "%.2fs");

        const bool nameEmpty = state.NewStateName[0] == '\0';
        const bool nameTaken = !nameEmpty && machine.GetStates().count(state.NewStateName) > 0;
        if (nameTaken) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "A state with that name already exists.");

        if (nameEmpty || nameTaken) ImGui::BeginDisabled();
        if (ImGui::Button("Create")) {
            std::shared_ptr<Animation> clip;
            if (state.NewStateClipPath[0] != '\0' && target.SourceModel) {
                const std::string resolved = AssetPaths::Resolve(AssetPaths::Category::Models, state.NewStateClipPath);
                clip = target.SourceModel->LoadAnimation(resolved);
                if (!clip) state.StatusMessage = "Failed to load clip - state created without one.";
            }
            machine.AddState(state.NewStateName, clip, state.NewStateBlendIn, state.NewStateClipPath);
            state.NodePositions[state.NewStateName] = state.PendingNewStatePos;
            if (machine.GetInitialState().empty()) {
                machine.SetInitialState(state.NewStateName);
            }
            ImGui::CloseCurrentPopup();
        }
        if (nameEmpty || nameTaken) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    auto& transitions = machine.GetTransitionsMutable();
    for (int i = 0; i < static_cast<int>(transitions.size()); ++i) {
        const auto& transition = transitions[i];
        if (transition.FromState == "*" || !machine.GetStates().count(transition.FromState) ||
            !machine.GetStates().count(transition.ToState)) {
            continue; // "any state" transitions are edited via the inspector, not drawn as an edge
        }

        ImVec2 from = nodeTopLeft(transition.FromState);
        from.x += nodeSize.x * 0.5f; from.y += nodeSize.y * 0.5f;
        ImVec2 to = nodeTopLeft(transition.ToState);
        to.x += nodeSize.x * 0.5f; to.y += nodeSize.y * 0.5f;

        const bool isSelected = (state.SelectedTransitionIndex == i);
        const ImU32 color = isSelected ? IM_COL32(255, 200, 60, 255) : IM_COL32(130, 130, 140, 255);
        drawList->AddLine(from, to, color, isSelected ? 3.0f : 1.5f);

        ImVec2 dir(to.x - from.x, to.y - from.y);
        const float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len > 1.0f) {
            dir.x /= len; dir.y /= len;
            const ImVec2 tip(to.x - dir.x * (nodeSize.x * 0.5f), to.y - dir.y * (nodeSize.y * 0.5f));
            const ImVec2 perp(-dir.y, dir.x);
            drawList->AddTriangleFilled(
                tip,
                ImVec2(tip.x - dir.x * 12.0f + perp.x * 6.0f, tip.y - dir.y * 12.0f + perp.y * 6.0f),
                ImVec2(tip.x - dir.x * 12.0f - perp.x * 6.0f, tip.y - dir.y * 12.0f - perp.y * 6.0f),
                color);
        }

        const ImVec2 mid((from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(mid.x - 8.0f, mid.y - 8.0f));
        ImGui::InvisibleButton("TransitionHit", ImVec2(16.0f, 16.0f));
        if (ImGui::IsItemClicked()) {
            state.SelectedTransitionIndex = i;
            state.SelectedState.clear();
        }
        ImGui::PopID();
        drawList->AddCircleFilled(mid, 5.0f, color);
    }

    for (const auto& [name, def] : machine.GetStates()) {
        const ImVec2 pos = nodeTopLeft(name);
        ImGui::PushID(name.c_str());
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("Node", nodeSize);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            state.NodePositions[name].x += delta.x;
            state.NodePositions[name].y += delta.y;
        }
        if (ImGui::IsItemClicked()) {
            state.SelectedState = name;
            state.SelectedTransitionIndex = -1;
        }
        ImGui::PopID();

        const bool isSelected = (state.SelectedState == name);
        const bool isRuntimeCurrent = machine.GetCurrentState() == name;
        const ImU32 fillColor = isRuntimeCurrent ? IM_COL32(65, 125, 85, 255) : IM_COL32(52, 52, 60, 255);
        const ImU32 borderColor = isSelected ? IM_COL32(255, 200, 60, 255) : IM_COL32(95, 95, 105, 255);

        drawList->AddRectFilled(pos, ImVec2(pos.x + nodeSize.x, pos.y + nodeSize.y), fillColor, 5.0f);
        drawList->AddRect(pos, ImVec2(pos.x + nodeSize.x, pos.y + nodeSize.y), borderColor, 5.0f, 0, isSelected ? 2.5f : 1.0f);
        drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 6.0f), IM_COL32(240, 240, 240, 255), name.c_str());

        char subLabel[80];
        std::snprintf(subLabel, sizeof(subLabel), "blend %.2fs", def.BlendInDuration);
        drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 26.0f), IM_COL32(180, 180, 185, 255), subLabel);

        if (machine.GetInitialState() == name) {
            drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 42.0f), IM_COL32(140, 200, 255, 255), "initial");
        }
    }

    ImGui::EndChild();
}

void EditorUI::DrawStateInspector(const AnimatorEditTarget& target, AnimatorEditorTargetState& state) {
    AnimationStateMachine& machine = *target.Machine;
    auto it = machine.GetStatesMutable().find(state.SelectedState);
    if (it == machine.GetStatesMutable().end()) { state.SelectedState.clear(); return; }

    ImGui::SeparatorText(("State: " + state.SelectedState).c_str());

    if (state.LastEditedState != state.SelectedState) {
        std::strncpy(state.RenameBuffer, state.SelectedState.c_str(), sizeof(state.RenameBuffer) - 1);
        std::strncpy(state.ClipPathBuffer, it->second.ClipPath.c_str(), sizeof(state.ClipPathBuffer) - 1);
        state.LastEditedState = state.SelectedState;
    }

    ImGui::InputText("Name", state.RenameBuffer, sizeof(state.RenameBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Rename")) {
        const std::string newName = state.RenameBuffer;
        if (machine.RenameState(state.SelectedState, newName)) {
            auto nodeIt = state.NodePositions.find(state.SelectedState);
            if (nodeIt != state.NodePositions.end()) {
                state.NodePositions[newName] = nodeIt->second;
                state.NodePositions.erase(nodeIt);
            }
            state.SelectedState = newName;
            state.LastEditedState = newName;
        } else {
            state.StatusMessage = "Rename failed (empty or duplicate name)";
        }
    }

    float blendIn = it->second.BlendInDuration;
    if (ImGui::SliderFloat("Blend In", &blendIn, 0.0f, 2.0f, "%.2fs")) {
        machine.SetStateBlendIn(state.SelectedState, blendIn);
    }

    ImGui::InputText("Clip Path", state.ClipPathBuffer, sizeof(state.ClipPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Load Clip") && target.SourceModel) {
        const std::string resolved = AssetPaths::Resolve(AssetPaths::Category::Models, state.ClipPathBuffer);
        auto clip = target.SourceModel->LoadAnimation(resolved);
        if (clip) {
            machine.SetStateClip(state.SelectedState, clip, state.ClipPathBuffer);
            state.StatusMessage = "Clip loaded.";
        } else {
            state.StatusMessage = "Failed to load clip - see log.";
        }
    }

    if (it->second.Clip && ImGui::Button("Preview This Clip")) {
        state.PreviewMode = true;
        target.AnimatorPtr->BeginPreview();
        target.AnimatorPtr->ScrubToNormalizedTime(it->second.Clip, state.PreviewNormalizedTime);
    }
    ImGui::SameLine();
    if (ImGui::Button("Set As Initial")) {
        machine.SetInitialState(state.SelectedState);
    }

    ImGui::Separator();
    if (ImGui::Button("Delete State")) {
        const std::string removedName = state.SelectedState;
        machine.RemoveState(removedName);
        state.NodePositions.erase(removedName);
        state.SelectedState.clear();
        state.LastEditedState.clear();
    }
}

void EditorUI::DrawTransitionInspector(const AnimatorEditTarget& target, AnimatorEditorTargetState& state) {
    AnimationStateMachine& machine = *target.Machine;
    auto& transitions = machine.GetTransitionsMutable();
    if (state.SelectedTransitionIndex < 0 || state.SelectedTransitionIndex >= static_cast<int>(transitions.size())) {
        state.SelectedTransitionIndex = -1;
        return;
    }
    auto& transition = transitions[state.SelectedTransitionIndex];

    ImGui::SeparatorText("Transition");

    if (ImGui::BeginCombo("From", transition.FromState.c_str())) {
        const bool wildcardSelected = (transition.FromState == "*");
        if (ImGui::Selectable("*", wildcardSelected)) transition.FromState = "*";
        for (const auto& [name, def] : machine.GetStates()) {
            const bool isSelected = (name == transition.FromState);
            if (ImGui::Selectable(name.c_str(), isSelected)) transition.FromState = name;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("To", transition.ToState.c_str())) {
        for (const auto& [name, def] : machine.GetStates()) {
            const bool isSelected = (name == transition.ToState);
            if (ImGui::Selectable(name.c_str(), isSelected)) transition.ToState = name;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Conditions (AND)");

    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(transition.Conditions.size()); ++i) {
        auto& condition = transition.Conditions[i];
        ImGui::PushID(i);

        char paramBuf[64];
        std::strncpy(paramBuf, condition.ParamName.c_str(), sizeof(paramBuf) - 1);
        paramBuf[sizeof(paramBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputText("##Param", paramBuf, sizeof(paramBuf))) condition.ParamName = paramBuf;

        ImGui::SameLine();
        int typeIndex = static_cast<int>(condition.Type);
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("##Type", &typeIndex, "bool\0float\0int\0")) {
            condition.Type = static_cast<AnimationStateMachine::ParamType>(typeIndex);
        }

        ImGui::SameLine();
        int compIndex = static_cast<int>(condition.Comp);
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::Combo("##Comp", &compIndex,
            "equals\0notEquals\0greaterThan\0lessThan\0greaterOrEqual\0lessOrEqual\0")) {
            condition.Comp = static_cast<AnimationStateMachine::Comparison>(compIndex);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        switch (condition.Type) {
            case AnimationStateMachine::ParamType::Bool:  ImGui::Checkbox("##Value", &condition.BoolValue); break;
            case AnimationStateMachine::ParamType::Float: ImGui::InputFloat("##Value", &condition.FloatValue); break;
            case AnimationStateMachine::ParamType::Int:   ImGui::InputInt("##Value", &condition.IntValue); break;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeIndex = i;

        ImGui::PopID();
    }
    if (removeIndex >= 0) transition.Conditions.erase(transition.Conditions.begin() + removeIndex);

    if (ImGui::Button("+ Add Condition")) {
        transition.Conditions.push_back(AnimationStateMachine::Condition{});
    }

    ImGui::Separator();
    if (ImGui::Button("Delete Transition")) {
        machine.RemoveTransition(static_cast<size_t>(state.SelectedTransitionIndex));
        state.SelectedTransitionIndex = -1;
    }
}

void EditorUI::DrawBoneTree(const AnimatorEditTarget& target, AnimatorEditorTargetState& state) {
    ImGui::BeginChild("BoneTree", ImVec2(0, 220), true);
    ImGui::TextUnformatted("Skeleton");
    ImGui::Separator();

    std::shared_ptr<Animation> referenceClip;
    if (!state.SelectedState.empty()) {
        auto it = target.Machine->GetStates().find(state.SelectedState);
        if (it != target.Machine->GetStates().end()) referenceClip = it->second.Clip;
    }
    if (!referenceClip) referenceClip = target.AnimatorPtr->GetCurrentAnimation();

    if (!referenceClip) {
        ImGui::TextDisabled("Select a state (or start playback) to see its skeleton.");
        ImGui::EndChild();
        return;
    }

    const auto& boneMap = referenceClip->GetBoneIDMap();

    std::function<void(const AssimpNodeData&)> drawNode = [&](const AssimpNodeData& node) {
        const bool isBone = boneMap.find(node.name) != boneMap.end();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (isBone) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.55f, 1.0f));

        std::string label = node.name.empty() ? "(unnamed)" : node.name;
        if (isBone) label += " [bone " + std::to_string(boneMap.at(node.name).id) + "]";

        const bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
        if (isBone) ImGui::PopStyleColor();

        if (opened && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
            for (const auto& child : node.children) drawNode(child);
            ImGui::TreePop();
        }
    };

    drawNode(referenceClip->GetRootNode());
    ImGui::EndChild();
}

void EditorUI::DrawAnimatorTimeline(const AnimatorEditTarget& target, AnimatorEditorTargetState& state) {
    ImGui::BeginChild("Timeline", ImVec2(0, 220), true);
    ImGui::TextUnformatted("Timeline");
    ImGui::Separator();

    std::shared_ptr<Animation> clip;
    if (!state.SelectedState.empty()) {
        auto it = target.Machine->GetStates().find(state.SelectedState);
        if (it != target.Machine->GetStates().end()) clip = it->second.Clip;
    }
    if (!clip) clip = target.AnimatorPtr->GetCurrentAnimation();

    if (!clip) {
        ImGui::TextDisabled("Select a state with a loaded clip to scrub its timeline.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("Duration: %.2f ticks @ %.1f tps", clip->GetDuration(), clip->GetTicksPerSecond());

    const bool scrubbable = state.PreviewMode;
    if (!scrubbable) ImGui::BeginDisabled();
    if (ImGui::SliderFloat("Playhead", &state.PreviewNormalizedTime, 0.0f, 1.0f)) {
        target.AnimatorPtr->ScrubToNormalizedTime(clip, state.PreviewNormalizedTime);
    }
    if (!scrubbable) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(enable Preview Mode to scrub)");
    }

    const ImVec2 barPos = ImGui::GetCursorScreenPos();
    const ImVec2 barSize(ImGui::GetContentRegionAvail().x, 16.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), IM_COL32(45, 45, 50, 255));

    for (const auto& event : clip->GetEvents()) {
        const float x = barPos.x + event.NormalizedTime * barSize.x;
        drawList->AddTriangleFilled(ImVec2(x - 5, barPos.y), ImVec2(x + 5, barPos.y),
                                     ImVec2(x, barPos.y + barSize.y), IM_COL32(230, 180, 60, 255));
    }
    ImGui::Dummy(barSize);

    ImGui::Separator();
    ImGui::TextUnformatted("Events");

    int removeEventIndex = -1;
    auto& mutableEvents = clip->GetEventsMutable();
    for (int i = 0; i < static_cast<int>(mutableEvents.size()); ++i) {
        ImGui::PushID(i);
        ImGui::Text("%.2f", mutableEvents[i].NormalizedTime);
        ImGui::SameLine();
        ImGui::TextUnformatted(mutableEvents[i].Name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) removeEventIndex = i;
        ImGui::PopID();
    }
    if (removeEventIndex >= 0) clip->RemoveEvent(static_cast<size_t>(removeEventIndex));

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("##NewEventName", state.NewEventName, sizeof(state.NewEventName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("##NewEventTime", &state.NewEventTime, 0.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("+ Add Event") && state.NewEventName[0] != '\0') {
        clip->AddEvent(state.NewEventName, state.NewEventTime);
        state.NewEventName[0] = '\0';
    }

    ImGui::EndChild();
}
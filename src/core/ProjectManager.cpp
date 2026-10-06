#include "ProjectManager.h"
#include "Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <array>

namespace ProjectManager {

namespace {
std::filesystem::path g_projectRoot;
bool g_projectOpen = false;
ProjectInfo g_currentProject;

const std::array<const char*, 8> kProjectAssetFolders = {
    "models", "textures", "materials", "scenes",
    "scripts", "shaders", "audio", "prefabs"
};

bool HasInvalidNameCharacter(const std::string& name) {
    return name.empty() || name.find_first_of("\\/<>:\"|?*") != std::string::npos;
}

std::filesystem::path ManifestPath(const std::filesystem::path& projectRoot) {
    return projectRoot / "project.json";
}
} // namespace

std::filesystem::path GetProjectRoot() {
    if (g_projectRoot.empty()) {
        return std::filesystem::current_path();
    }
    return g_projectRoot;
}

bool IsProjectOpen() { return g_projectOpen; }

std::string GetProjectName() {
    return g_projectOpen ? g_currentProject.Name : std::string();
}

bool CreateNewProject(const std::filesystem::path& parentDirectory, const std::string& projectName,
                      std::string& outError) {
    if (HasInvalidNameCharacter(projectName)) {
        outError = "Enter a valid project name";
        return false;
    }

    std::error_code error;
    const std::filesystem::path projectRoot = parentDirectory / projectName;

    if (std::filesystem::exists(projectRoot, error)) {
        bool isEmpty = true;
        for (const auto& entry : std::filesystem::directory_iterator(projectRoot, error)) {
            (void)entry;
            isEmpty = false;
            break;
        }
        if (!isEmpty) {
            outError = "A non-empty folder already exists at that location";
            return false;
        }
    }

    std::filesystem::create_directories(projectRoot, error);
    if (error) {
        outError = "Could not create project folder: " + error.message();
        return false;
    }

    for (const char* folder : kProjectAssetFolders) {
        std::filesystem::create_directories(projectRoot / folder, error);
        if (error) {
            Log::Warn("ProjectManager: failed to create '{}' in new project: {}", folder, error.message());
            error.clear();
        }
    }

    nlohmann::json manifest;
    manifest["name"] = projectName;
    manifest["engineVersion"] = "1.0";
    // No <chrono>/date formatting dependency added here on purpose — a
    // human can fill this in by hand, or a future pass can wire real
    // timestamp formatting through this same field without changing the
    // manifest schema.
    manifest["created"] = "";

    std::ofstream file(ManifestPath(projectRoot));
    if (!file.is_open()) {
        outError = "Could not write project.json";
        return false;
    }
    file << manifest.dump(2);
    file.close();

    g_projectRoot = projectRoot;
    g_projectOpen = true;
    g_currentProject.Name = projectName;
    g_currentProject.EngineVersion = "1.0";
    g_currentProject.CreatedDate = "";

    Log::Info("ProjectManager: created new project '{}' at '{}'", projectName, projectRoot.string());
    return true;
}

bool OpenProject(const std::filesystem::path& projectRootPath, std::string& outError) {
    std::error_code error;
    const std::filesystem::path manifestPath = ManifestPath(projectRootPath);

    if (!std::filesystem::exists(manifestPath, error)) {
        outError = "No project.json found at that location";
        return false;
    }

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        outError = "Could not open project.json";
        return false;
    }

    nlohmann::json manifest;
    try {
        file >> manifest;
    } catch (const std::exception& e) {
        outError = std::string("Failed to parse project.json: ") + e.what();
        return false;
    }

    g_projectRoot = std::filesystem::absolute(projectRootPath, error);
    if (error) {
        g_projectRoot = projectRootPath;
    }
    g_projectOpen = true;
    g_currentProject.Name = manifest.value("name", projectRootPath.filename().string());
    g_currentProject.EngineVersion = manifest.value("engineVersion", "1.0");
    g_currentProject.CreatedDate = manifest.value("created", "");

    Log::Info("ProjectManager: opened project '{}' at '{}'", g_currentProject.Name, g_projectRoot.string());
    return true;
}

void CloseProject() {
    g_projectRoot.clear();
    g_projectOpen = false;
    g_currentProject = ProjectInfo{};
}

} // namespace ProjectManager
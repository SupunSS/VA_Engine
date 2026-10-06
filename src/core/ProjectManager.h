#pragma once
#include <filesystem>
#include <string>

// Owns the concept of "which project is currently open" — a single global
// root directory that AssetPaths resolves everything relative to.
//
// Defaults to the current working directory until a project is explicitly
// created/opened, which is deliberate: the existing `engine` dev-sandbox
// target (src/main.cpp) never calls into this at all, so it keeps working
// completely unchanged, resolving assets from wherever it's launched —
// exactly like before this system existed. Only the `game` target (or a
// future dedicated project-picker flow) needs to actually call
// CreateNewProject/OpenProject.
namespace ProjectManager {

struct ProjectInfo {
    std::string Name;
    std::string EngineVersion = "1.0";
    std::string CreatedDate;
};

// Returns the active project root. Before any project is opened, this is
// simply std::filesystem::current_path() — same behavior AssetPaths always
// had.
std::filesystem::path GetProjectRoot();

bool IsProjectOpen();
std::string GetProjectName();

// Creates a brand-new project: parentDirectory/projectName, populated with
// the standard asset subfolders (models/, textures/, audio/, scenes/,
// scripts/, shaders/, prefabs/, materials/) and a project.json manifest,
// then makes it the active project root. Fails if projectName is empty,
// contains invalid characters, or the target folder already exists and is
// non-empty.
bool CreateNewProject(const std::filesystem::path& parentDirectory, const std::string& projectName,
                      std::string& outError);

// Opens an existing project by its root folder (the one directly
// containing project.json). Fails if project.json is missing or fails to
// parse — does NOT fall back to treating an arbitrary folder as a project,
// since that would silently accept a non-project directory.
bool OpenProject(const std::filesystem::path& projectRootPath, std::string& outError);

// Reverts to the CWD-based default root. Mainly useful for tests/tooling;
// not exposed in the editor UI today.
void CloseProject();

} // namespace ProjectManager
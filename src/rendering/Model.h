#pragma once
#include "Mesh.h"
#include "AnimData.h"
#include <assimp/scene.h>
#include <filesystem>
#include <vector>
#include <map>
#include <string>
#include <memory>

class Animation;

class Model {
public:
    explicit Model(const std::string& path);

    void Draw(const Shader& shader, const Material* overrideMaterial = nullptr) const;
    void SetMaterial(std::shared_ptr<Material> material);

    // Loads an additional animation clip (e.g. a separate walk.fbx) against
    // this model's existing skeleton, sharing bone IDs so both clips drive
    // the same skinning matrices.
    std::shared_ptr<Animation> LoadAnimation(const std::string& path);

    std::map<std::string, BoneInfo>& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCount; }

private:
    void LoadModel(const std::string& path);
    void ProcessNode(aiNode* node, const aiScene* scene);
    std::unique_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
    std::shared_ptr<Material> LoadMaterialForMesh(aiMesh* mesh, const aiScene* scene);
    void ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::filesystem::path m_directory;

    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCount = 0;
};
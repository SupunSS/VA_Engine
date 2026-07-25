#pragma once
#include "Mesh.h"
#include "AnimData.h"
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <vector>
#include <map>
#include <string>
#include <memory>

class Animation;

class Model {
public:
    // 1. Path-based constructor (used for loading .obj/.fbx/.gltf assets from disk)
    explicit Model(const std::string& path);

    // 2. Procedural constructor (used by Primitives.cpp for generating shapes with explicit bounds)
    Model(std::vector<std::unique_ptr<Mesh>> meshes, const glm::vec3& boundsMin, const glm::vec3& boundsMax);

    void Draw(const Shader& shader, const Material* overrideMaterial = nullptr) const;

    // Instanced counterpart — see Mesh::DrawInstanced for the actual detail.
    // shader must be the instanced vertex shader variant.
    void DrawInstanced(const Shader& shader, const Material* overrideMaterial,
                        const std::vector<glm::mat4>& instanceMatrices) const;

    void SetMaterial(std::shared_ptr<Material> material);

    // Loads an additional animation clip (e.g. a separate walk.fbx) against
    // this model's existing skeleton, sharing bone IDs so both clips drive
    // the same skinning matrices.
    std::shared_ptr<Animation> LoadAnimation(const std::string& path);

    // Getters for skeletal data
    std::map<std::string, BoneInfo>& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCount; }

    // Getters for bounding boxes (useful for gizmos, ray picking, and physics sizing)
    const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
    const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

private:
    void LoadModel(const std::string& path);
    void ProcessNode(aiNode* node, const aiScene* scene);
    std::unique_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
    std::shared_ptr<Material> LoadMaterialForMesh(aiMesh* mesh, const aiScene* scene);
    void ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);

    std::vector<std::unique_ptr<Mesh>> m_meshes;
    std::filesystem::path m_directory;

    // Skeletal animation tracking
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCount = 0;

    // Spatial bounding box tracking (resolves the undeclared identifier errors)
    glm::vec3 m_boundsMin{ 0.0f };
    glm::vec3 m_boundsMax{ 0.0f };
};
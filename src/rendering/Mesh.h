#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "Shader.h"
#include "Material.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec3 Tangent;

    static const int MAX_BONE_INFLUENCE = 4;
    int   BoneIDs[MAX_BONE_INFLUENCE]     = { -1, -1, -1, -1 };
    float BoneWeights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Called while building the mesh, once per (vertex, bone) influence found
    // in the Assimp data. Ignores extra influences beyond MAX_BONE_INFLUENCE —
    // Assimp doesn't guarantee weights arrive sorted by strength, but in
    // practice skeletal rigs rarely assign more than 4 meaningful influences
    // per vertex, so silently dropping the rest is the standard tradeoff.
    void AddBoneData(int boneID, float weight) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (BoneWeights[i] == 0.0f) {
                BoneIDs[i] = boneID;
                BoneWeights[i] = weight;
                return;
            }
        }
    }
};

class Mesh {
public:
    // Non-const: Model::ProcessMesh calls ExtractBoneWeights on this vector
    // (mutating BoneIDs/BoneWeights per vertex) before passing it in here,
    // so the vertex data arriving at the constructor is already final by
    // the time it's uploaded to the GPU.
    Mesh(std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    void Draw(const Shader& shader, const Material* overrideMaterial = nullptr) const;

    std::shared_ptr<Material> material;

private:
    void SetupMesh();

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;

    unsigned int m_VAO, m_VBO, m_EBO;
};
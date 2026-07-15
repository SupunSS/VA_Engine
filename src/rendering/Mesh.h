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
};

class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    void Draw(const Shader& shader, const Material* overrideMaterial = nullptr) const;

    std::shared_ptr<Material> material;

private:
    void SetupMesh();

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;

    unsigned int m_VAO, m_VBO, m_EBO;
};
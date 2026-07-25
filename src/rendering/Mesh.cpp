#include "Mesh.h"
#include <glad/glad.h>

Mesh::Mesh(std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_vertices(vertices), m_indices(indices) {
    SetupMesh();
}

Mesh::~Mesh() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
    if (m_instanceVBO != 0) {
        glDeleteBuffers(1, &m_instanceVBO);
    }
}

void Mesh::SetupMesh() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // texcoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoord));

    // tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

    // bone IDs — integer attribute, must use glVertexAttribIPointer (not the
    // float version) or the driver silently reinterprets the int bits as floats
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, BoneIDs));

    // bone weights
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, BoneWeights));

    // --- Per-instance model matrix (locations 6-9) --------------------
    // A mat4 vertex attribute must be split into 4 consecutive vec4
    // locations — this is a hard OpenGL constraint (no single-location
    // mat4 attribute type). glVertexAttribDivisor(loc, 1) is what makes
    // these advance once per INSTANCE instead of once per VERTEX; without
    // it this would just be garbage per-vertex data. Data itself isn't
    // uploaded here — DrawInstanced() fills m_instanceVBO fresh each call,
    // since which entities are actually being batched changes every frame
    // as culling results change.
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    for (unsigned int i = 0; i < 4; ++i) {
        const unsigned int location = 6 + i;
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
            (void*)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(location, 1);
    }

    glBindVertexArray(0);
}

void Mesh::Draw(const Shader& shader, const Material* overrideMaterial) const {
    const Material* activeMaterial = overrideMaterial ? overrideMaterial : material.get();
    if (activeMaterial) {
        activeMaterial->Bind(shader);
    }

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(m_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Mesh::DrawInstanced(const Shader& shader, const Material* overrideMaterial,
                          const std::vector<glm::mat4>& instanceMatrices) const {
    if (instanceMatrices.empty()) {
        return;
    }

    const Material* activeMaterial = overrideMaterial ? overrideMaterial : material.get();
    if (activeMaterial) {
        activeMaterial->Bind(shader);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size() * sizeof(glm::mat4),
        instanceMatrices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(m_VAO);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(m_indices.size()), GL_UNSIGNED_INT, 0,
        static_cast<int>(instanceMatrices.size()));
    glBindVertexArray(0);
}
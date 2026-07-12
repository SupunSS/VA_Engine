#pragma once
#include "Mesh.h"
#include <assimp/scene.h>
#include <vector>
#include <string>
#include <memory>

class Model {
public:
    explicit Model(const std::string& path);

    void Draw() const;

private:
    void LoadModel(const std::string& path);
    void ProcessNode(aiNode* node, const aiScene* scene);
    std::unique_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);

    std::vector<std::unique_ptr<Mesh>> m_meshes;
};
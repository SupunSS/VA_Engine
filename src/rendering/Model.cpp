#include "Model.h"
#include "ResourceManager.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <filesystem>

namespace {
std::filesystem::path ResolveTexturePath(const std::filesystem::path& modelDirectory, const std::string& textureReference) {
    std::filesystem::path texturePath(textureReference);
    std::error_code ec;

    auto normalize = [&](const std::filesystem::path& path) {
        std::error_code localEc;
        auto canonical = std::filesystem::weakly_canonical(path, localEc);
        if (!localEc) {
            return canonical.lexically_normal();
        }
        auto absolutePath = std::filesystem::absolute(path, localEc);
        return absolutePath.lexically_normal();
    };

    if (texturePath.is_absolute()) {
        auto normalized = normalize(texturePath);
        if (std::filesystem::exists(normalized, ec)) {
            return normalized;
        }
        return normalized;
    }

    // Try the path relative to the model directory first.
    auto candidate = normalize(modelDirectory / texturePath);
    if (!ec && std::filesystem::exists(candidate, ec)) {
        return candidate;
    }

    // Try the top-level textures folder alongside the models folder.
    auto rootTextures = normalize(modelDirectory.parent_path() / "textures" / texturePath);
    if (!ec && std::filesystem::exists(rootTextures, ec)) {
        return rootTextures;
    }

    // Try the current working directory textures folder.
    auto cwdTextures = normalize(std::filesystem::current_path() / "textures" / texturePath);
    if (!ec && std::filesystem::exists(cwdTextures, ec)) {
        return cwdTextures;
    }

    return candidate;
}
}

Model::Model(const std::string& path) {
    LoadModel(path);
}

void Model::LoadModel(const std::string& path) {
    m_directory = std::filesystem::path(path).parent_path();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        Log::Warn("Failed to load model {}: {}", path, importer.GetErrorString());
        return;
    }

    Log::Info("Model loaded: {} ({} meshes)", path, scene->mNumMeshes);

    ProcessNode(scene->mRootNode, scene);
}

void Model::ProcessNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(ProcessMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene);
    }
}

std::shared_ptr<Material> Model::LoadMaterialForMesh(aiMesh* mesh, const aiScene* scene) {
    auto material = std::make_shared<Material>();

    if (mesh->mMaterialIndex >= scene->mNumMaterials) {
        return material;
    }

    aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];

    aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
        material->albedoTint = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
    }

    aiString texPath;
    if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
        aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
        auto resolvedPath = ResolveTexturePath(m_directory, texPath.C_Str());
        Log::Info("Resolved albedo texture path: {}", resolvedPath.string());
        if (std::filesystem::exists(resolvedPath)) {
            material->albedoMap = ResourceManager::LoadTexture(resolvedPath.string());
        } else {
            Log::Warn("Albedo texture not found: {}", resolvedPath.string());
        }
    }

    // OBJ's map_Bump is usually imported by assimp as HEIGHT; fall back to NORMALS
    aiTextureType bumpType = aiTextureType_HEIGHT;
    if (aiMat->GetTextureCount(bumpType) == 0) {
        bumpType = aiTextureType_NORMALS;
    }
    if (aiMat->GetTextureCount(bumpType) > 0 &&
        aiMat->GetTexture(bumpType, 0, &texPath) == AI_SUCCESS) {
        auto resolvedPath = ResolveTexturePath(m_directory, texPath.C_Str());
        Log::Info("Resolved normal texture path: {}", resolvedPath.string());
        if (std::filesystem::exists(resolvedPath)) {
            material->normalMap = ResourceManager::LoadTexture(resolvedPath.string());
        } else {
            Log::Warn("Normal texture not found: {}", resolvedPath.string());
        }
    }

    return material;
}

std::unique_ptr<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->mTextureCoords[0]) {
            vertex.TexCoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.TexCoord = glm::vec2(0.0f, 0.0f);
        }

        if (mesh->HasTangentsAndBitangents()) {
            vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
        } else {
            vertex.Tangent = glm::vec3(0.0f);
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    auto meshObj = std::make_unique<Mesh>(vertices, indices);
    meshObj->material = LoadMaterialForMesh(mesh, scene);
    return meshObj;
}

void Model::Draw(const Shader& shader, const Material* overrideMaterial) const {
    for (const auto& mesh : m_meshes) {
        mesh->Draw(shader, overrideMaterial);
    }
}

void Model::SetMaterial(std::shared_ptr<Material> material) {
    for (auto& mesh : m_meshes) {
        mesh->material = material;
    }
}
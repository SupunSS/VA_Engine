#include "Model.h"
#include "Animation.h"
#include "ResourceManager.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/anim.h>
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

glm::mat4 ConvertMatrix(const aiMatrix4x4& m) {
    glm::mat4 result;
    result[0][0] = m.a1; result[1][0] = m.a2; result[2][0] = m.a3; result[3][0] = m.a4;
    result[0][1] = m.b1; result[1][1] = m.b2; result[2][1] = m.b3; result[3][1] = m.b4;
    result[0][2] = m.c1; result[1][2] = m.c2; result[2][2] = m.c3; result[3][2] = m.c4;
    result[0][3] = m.d1; result[1][3] = m.d2; result[2][3] = m.d3; result[3][3] = m.d4;
    return result;
}
}

Model::Model(const std::string& path) {
    LoadModel(path);
}

// Procedural constructor — used by Primitives.cpp for generating shapes
// (spheres, boxes, etc. for physics testing) with explicit precomputed
// bounds, bypassing the Assimp load path entirely.
Model::Model(std::vector<std::unique_ptr<Mesh>> meshes, const glm::vec3& boundsMin, const glm::vec3& boundsMax)
    : m_meshes(std::move(meshes)), m_boundsMin(boundsMin), m_boundsMax(boundsMax) {
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

void Model::ExtractBoneWeights(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene) {
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        aiBone* bone = mesh->mBones[boneIndex];
        std::string boneName = bone->mName.C_Str();

        int boneID;
        auto it = m_BoneInfoMap.find(boneName);
        if (it == m_BoneInfoMap.end()) {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCount;
            newBoneInfo.offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = m_BoneCount;
            m_BoneCount++;
        } else {
            boneID = it->second.id;
        }

        // Each weight entry tells us: "this bone influences this vertex by
        // this much" — the inverse of how the Vertex struct stores it
        // (vertex -> list of bones), so we scatter into vertices here.
        aiVertexWeight* weights = bone->mWeights;
        for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
            unsigned int vertexId = weights[weightIndex].mVertexId;
            float weight = weights[weightIndex].mWeight;
            if (vertexId < vertices.size()) {
                vertices[vertexId].AddBoneData(boneID, weight);
            }
        }
    }
}

std::unique_ptr<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        // Accumulate local-space bounds across every mesh in this model.
        m_boundsMin = glm::min(m_boundsMin, vertex.Position);
        m_boundsMax = glm::max(m_boundsMax, vertex.Position);

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

        // BoneIDs/BoneWeights already default to {-1,-1,-1,-1} / {0,0,0,0}
        // via the in-class initializers — ExtractBoneWeights fills them in
        // below for meshes that actually have a skeleton.
        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    ExtractBoneWeights(vertices, mesh, scene);

    auto meshObj = std::make_unique<Mesh>(vertices, indices);
    meshObj->material = LoadMaterialForMesh(mesh, scene);
    return meshObj;
}

std::shared_ptr<Animation> Model::LoadAnimation(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);

    if (!scene || !scene->mRootNode || scene->mNumAnimations == 0) {
        Log::Warn("Failed to load animation {}: {}", path, importer.GetErrorString());
        return nullptr;
    }

    auto animation = std::make_shared<Animation>(scene, scene->mAnimations[0], m_BoneInfoMap, m_BoneCount);
    animation->SetSourcePath(path);
    return animation;
}

void Model::Draw(const Shader& shader, const Material* overrideMaterial) const {
    for (const auto& mesh : m_meshes) {
        mesh->Draw(shader, overrideMaterial);
    }
}

void Model::DrawInstanced(const Shader& shader, const Material* overrideMaterial,
                           const std::vector<glm::mat4>& instanceMatrices) const {
    for (const auto& mesh : m_meshes) {
        mesh->DrawInstanced(shader, overrideMaterial, instanceMatrices);
    }
}

void Model::SetMaterial(std::shared_ptr<Material> material) {
    for (auto& mesh : m_meshes) {
        mesh->material = material;
    }
}
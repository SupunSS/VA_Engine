#include "ResourceManager.h"

std::unordered_map<std::string, std::shared_ptr<Texture>> ResourceManager::s_textures;
std::unordered_map<std::string, std::shared_ptr<Model>> ResourceManager::s_models;

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& path) {
    auto it = s_textures.find(path);
    if (it != s_textures.end()) return it->second;

    auto texture = std::make_shared<Texture>(path);
    s_textures[path] = texture;
    return texture;
}

std::shared_ptr<Model> ResourceManager::LoadModel(const std::string& path) {
    auto it = s_models.find(path);
    if (it != s_models.end()) return it->second;

    auto model = std::make_shared<Model>(path);
    s_models[path] = model;
    return model;
}
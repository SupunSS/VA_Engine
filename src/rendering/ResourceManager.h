#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "Texture.h"
#include "Model.h"

class ResourceManager {
public:
    static std::shared_ptr<Texture> LoadTexture(const std::string& path);
    static std::shared_ptr<Model> LoadModel(const std::string& path);

private:
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_textures;
    static std::unordered_map<std::string, std::shared_ptr<Model>> s_models;
};
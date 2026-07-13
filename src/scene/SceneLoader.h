#pragma once
#include "Scene.h"
#include <string>
#include <unordered_map>
#include <memory>

class Model; // forward declare

class SceneLoader {
public:
    static void LoadFromFile(const std::string& path, Scene& scene);

private:
    // Cache so identical model paths share one GPU-side Model instance
    static std::unordered_map<std::string, std::shared_ptr<Model>> s_modelCache;
};
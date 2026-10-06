#pragma once
#include <memory>
#include <glm/glm.hpp>
#include "Texture.h"
#include "Shader.h"

class Material {
public:
    std::shared_ptr<Texture> albedoMap;
    std::shared_ptr<Texture> normalMap;
    std::shared_ptr<Texture> roughnessMap;
    std::shared_ptr<Texture> metallicMap;

    glm::vec3 albedoTint = glm::vec3(1.0f);
    float roughness = 0.5f;
    float metallic = 0.0f;
    float normalStrength = 1.0f;

    void Bind(const Shader& shader) const;
};
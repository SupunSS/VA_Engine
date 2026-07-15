#include "Material.h"

void Material::Bind(const Shader& shader) const {
    if (albedoMap) {
        albedoMap->Bind(0);
        shader.SetInt("uAlbedoMap", 0);
        shader.SetInt("uHasAlbedoMap", 1);
    } else {
        shader.SetInt("uHasAlbedoMap", 0);
    }

    if (normalMap) {
        normalMap->Bind(1);
        shader.SetInt("uNormalMap", 1);
        shader.SetInt("uHasNormalMap", 1);
    } else {
        shader.SetInt("uHasNormalMap", 0);
    }

    if (roughnessMap) {
        roughnessMap->Bind(2);
        shader.SetInt("uRoughnessMap", 2);
        shader.SetInt("uHasRoughnessMap", 1);
    } else {
        shader.SetInt("uHasRoughnessMap", 0);
    }

    if (metallicMap) {
        metallicMap->Bind(3);
        shader.SetInt("uMetallicMap", 3);
        shader.SetInt("uHasMetallicMap", 1);
    } else {
        shader.SetInt("uHasMetallicMap", 0);
    }

    shader.SetVec3("uAlbedoTint", albedoTint);
    shader.SetFloat("uRoughness", roughness);
    shader.SetFloat("uMetallic", metallic);
    shader.SetFloat("uNormalStrength", normalStrength);
}
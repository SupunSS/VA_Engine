#include "Material.h"

void Material::Bind(const Shader& shader) const {
    // A real Material object is bound here, even if it has no texture — so
    // this is never the checkerboard-fallback case. See triangle.frag /
    // Mesh::Draw's ResetMaterialUniformsToDefault for the genuinely-no-
    // material case, which is the only place this should be set to 1.
    shader.SetInt("uUseCheckerFallback", 0);

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
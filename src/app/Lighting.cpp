#include "Lighting.h"
#include <cmath>

glm::vec3 ComputeSunLightColor(const glm::vec3& sunDirection)
{
    const glm::vec3 kRayleighCoeff(5.5e-6f, 13.0e-6f, 22.4e-6f);
    const float kMieCoeff = 21e-6f * 1.1f;
    const float kPathReference = 8000.0f;

    float sinElevation = glm::max(sunDirection.y, 0.01f);
    float pathLength = kPathReference / sinElevation;

    glm::vec3 extinction = kRayleighCoeff + glm::vec3(kMieCoeff);
    glm::vec3 transmittance(
        std::exp(-extinction.x * pathLength),
        std::exp(-extinction.y * pathLength),
        std::exp(-extinction.z * pathLength));

    const glm::vec3 kBaseSunColor(1.0f, 0.96f, 0.9f);
    return kBaseSunColor * transmittance;
}
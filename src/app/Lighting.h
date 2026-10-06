#pragma once
#include <glm/glm.hpp>

// Sun colour after atmospheric extinction: warmer/dimmer as the sun nears the horizon.
glm::vec3 ComputeSunLightColor(const glm::vec3& sunDirection);
#include "Primitives.h"
#include "Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>

namespace Primitives {

std::shared_ptr<Model> CreateSphere(float radius, int latitudeSegments, int longitudeSegments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Standard UV sphere: a grid of (latitudeSegments+1) x (longitudeSegments+1)
    // vertices, with the top row collapsed to the north pole and the bottom
    // row collapsed to the south pole. Position/Normal point the same
    // direction for a sphere centered at the origin, since the surface
    // normal at any point IS the (normalized) position vector.
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        // theta sweeps 0 (north pole) to PI (south pole)
        float theta = static_cast<float>(lat) / static_cast<float>(latitudeSegments) * glm::pi<float>();
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            // phi sweeps 0 to 2*PI around the vertical axis
            float phi = static_cast<float>(lon) / static_cast<float>(longitudeSegments) * glm::two_pi<float>();
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            glm::vec3 direction(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);

            Vertex vertex;
            vertex.Position = direction * radius;
            vertex.Normal = direction; // already unit-length since direction is built from sin/cos
            vertex.TexCoord = glm::vec2(
                static_cast<float>(lon) / static_cast<float>(longitudeSegments),
                static_cast<float>(lat) / static_cast<float>(latitudeSegments)
            );
            // Tangent points along increasing longitude (phi), matching the
            // UV's U direction — consistent with how normal maps expect
            // tangent space to be built.
            vertex.Tangent = glm::vec3(-sinPhi, 0.0f, cosPhi);

            vertices.push_back(vertex);
        }
    }

    // Two triangles per grid quad, connecting each ring to the next.
    const unsigned int verticesPerRow = static_cast<unsigned int>(longitudeSegments + 1);
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            unsigned int current = static_cast<unsigned int>(lat) * verticesPerRow + static_cast<unsigned int>(lon);
            unsigned int next = current + verticesPerRow;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(std::make_unique<Mesh>(vertices, indices));

    // A sphere's bounds are trivially exact — no need to scan vertices.
    const glm::vec3 boundsMin(-radius);
    const glm::vec3 boundsMax(radius);

    return std::make_shared<Model>(std::move(meshes), boundsMin, boundsMax);
}

std::shared_ptr<Model> CreatePyramid(float baseHalfWidth, float height) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float halfHeight = height * 0.5f;

    const glm::vec3 apex(0.0f, halfHeight, 0.0f);
    const glm::vec3 base0(-baseHalfWidth, -halfHeight, -baseHalfWidth);
    const glm::vec3 base1( baseHalfWidth, -halfHeight, -baseHalfWidth);
    const glm::vec3 base2( baseHalfWidth, -halfHeight,  baseHalfWidth);
    const glm::vec3 base3(-baseHalfWidth, -halfHeight,  baseHalfWidth);

    // Adds one flat-shaded triangle: all three vertices share the same
    // face normal (computed from the triangle itself, not looked up), so
    // each face of the pyramid reads as a distinct flat facet rather than
    // being smoothed into its neighbors.
    auto addTriangle = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                            const glm::vec2& uvA, const glm::vec2& uvB, const glm::vec2& uvC) {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));

        // The pyramid's volume contains the origin for any positive
        // baseHalfWidth/height, so "does this normal point away from the
        // origin" reliably tells us whether it's actually outward-facing —
        // this avoids having to hand-verify winding order for every face.
        const glm::vec3 faceCenter = (a + b + c) / 3.0f;
        if (glm::dot(normal, faceCenter) < 0.0f) {
            normal = -normal;
        }

        const glm::vec3 tangent = glm::normalize(b - a);

        Vertex vA; vA.Position = a; vA.Normal = normal; vA.TexCoord = uvA; vA.Tangent = tangent;
        Vertex vB; vB.Position = b; vB.Normal = normal; vB.TexCoord = uvB; vB.Tangent = tangent;
        Vertex vC; vC.Position = c; vC.Normal = normal; vC.TexCoord = uvC; vC.Tangent = tangent;

        const unsigned int startIndex = static_cast<unsigned int>(vertices.size());
        vertices.push_back(vA);
        vertices.push_back(vB);
        vertices.push_back(vC);
        indices.push_back(startIndex);
        indices.push_back(startIndex + 1);
        indices.push_back(startIndex + 2);
    };

    // Four triangular side faces.
    addTriangle(base0, base1, apex, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.5f, 1.0f));
    addTriangle(base1, base2, apex, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.5f, 1.0f));
    addTriangle(base2, base3, apex, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.5f, 1.0f));
    addTriangle(base3, base0, apex, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.5f, 1.0f));

    // Square base, split into two triangles.
    addTriangle(base0, base2, base1, glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f));
    addTriangle(base0, base3, base2, glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f));

    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(std::make_unique<Mesh>(vertices, indices));

    const glm::vec3 boundsMin(-baseHalfWidth, -halfHeight, -baseHalfWidth);
    const glm::vec3 boundsMax(baseHalfWidth, halfHeight, baseHalfWidth);

    return std::make_shared<Model>(std::move(meshes), boundsMin, boundsMax);
}

} // namespace Primitives
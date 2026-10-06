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

std::shared_ptr<Model> CreateWheel(float radius, float width, int segments) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float halfWidth = width * 0.5f;

    // 1. Mantle (Cylinder outer surface)
    for (int i = 0; i <= segments; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(segments)) * glm::two_pi<float>();
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        glm::vec3 normal(0.0f, cosA, sinA); // Radial normal out from X-axis
        glm::vec3 tangent(1.0f, 0.0f, 0.0f); // Tangent along X-axis

        // Left rim vertex (-halfWidth)
        Vertex vLeft;
        vLeft.Position = glm::vec3(-halfWidth, radius * cosA, radius * sinA);
        vLeft.Normal = normal;
        vLeft.Tangent = tangent;
        vLeft.TexCoord = glm::vec2(static_cast<float>(i) / segments, 0.0f);

        // Right rim vertex (+halfWidth)
        Vertex vRight;
        vRight.Position = glm::vec3(halfWidth, radius * cosA, radius * sinA);
        vRight.Normal = normal;
        vRight.Tangent = tangent;
        vRight.TexCoord = glm::vec2(static_cast<float>(i) / segments, 1.0f);

        vertices.push_back(vLeft);
        vertices.push_back(vRight);
    }

    for (int i = 0; i < segments; ++i) {
        unsigned int baseIdx = i * 2;
        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);

        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 3);
        indices.push_back(baseIdx + 2);
    }

    // 2. Caps (Left Cap: x = -halfWidth, Normal = -X; Right Cap: x = +halfWidth, Normal = +X)
    auto addCap = [&](float xPos, const glm::vec3& normal, bool counterClockwise) {
        unsigned int centerIdx = static_cast<unsigned int>(vertices.size());
        Vertex vCenter;
        vCenter.Position = glm::vec3(xPos, 0.0f, 0.0f);
        vCenter.Normal = normal;
        vCenter.Tangent = glm::vec3(0.0f, 1.0f, 0.0f);
        vCenter.TexCoord = glm::vec2(0.5f, 0.5f);
        vertices.push_back(vCenter);

        unsigned int ringStart = static_cast<unsigned int>(vertices.size());
        for (int i = 0; i < segments; ++i) {
            float angle = (static_cast<float>(i) / static_cast<float>(segments)) * glm::two_pi<float>();
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            Vertex vRim;
            vRim.Position = glm::vec3(xPos, radius * cosA, radius * sinA);
            vRim.Normal = normal;
            vRim.Tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            vRim.TexCoord = glm::vec2(0.5f + 0.5f * cosA, 0.5f + 0.5f * sinA);
            vertices.push_back(vRim);
        }

        for (int i = 0; i < segments; ++i) {
            unsigned int current = ringStart + i;
            unsigned int next = ringStart + ((i + 1) % segments);
            if (counterClockwise) {
                indices.push_back(centerIdx);
                indices.push_back(next);
                indices.push_back(current);
            } else {
                indices.push_back(centerIdx);
                indices.push_back(current);
                indices.push_back(next);
            }
        }
    };

    addCap(-halfWidth, glm::vec3(-1.0f, 0.0f, 0.0f), false);
    addCap(halfWidth, glm::vec3(1.0f, 0.0f, 0.0f), true);

    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(std::make_unique<Mesh>(vertices, indices));

    const glm::vec3 boundsMin(-halfWidth, -radius, -radius);
    const glm::vec3 boundsMax(halfWidth, radius, radius);

    return std::make_shared<Model>(std::move(meshes), boundsMin, boundsMax);
}

std::shared_ptr<Model> CreateBox(float halfX, float halfY, float halfZ) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Helper: add a flat-shaded quad as 2 triangles.
    // a,b,c,d are the 4 corners in CCW winding when viewed from outside.
    auto addFace = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        glm::vec3 tangent = glm::normalize(b - a);
        auto makeV = [&](glm::vec3 p, glm::vec2 uv) {
            Vertex v; v.Position = p; v.Normal = normal;
            v.Tangent = tangent; v.TexCoord = uv;
            return v;
        };
        unsigned int base = static_cast<unsigned int>(vertices.size());
        vertices.push_back(makeV(a, {0,0}));
        vertices.push_back(makeV(b, {1,0}));
        vertices.push_back(makeV(c, {1,1}));
        vertices.push_back(makeV(d, {0,1}));
        indices.insert(indices.end(), {base,base+1,base+2, base,base+2,base+3});
    };

    // +Z face (front)
    addFace({-halfX,-halfY, halfZ},{halfX,-halfY, halfZ},{halfX, halfY, halfZ},{-halfX, halfY, halfZ});
    // -Z face (back)
    addFace({ halfX,-halfY,-halfZ},{-halfX,-halfY,-halfZ},{-halfX, halfY,-halfZ},{ halfX, halfY,-halfZ});
    // +Y face (top)
    addFace({-halfX, halfY, halfZ},{halfX, halfY, halfZ},{halfX, halfY,-halfZ},{-halfX, halfY,-halfZ});
    // -Y face (bottom)
    addFace({-halfX,-halfY,-halfZ},{halfX,-halfY,-halfZ},{halfX,-halfY, halfZ},{-halfX,-halfY, halfZ});
    // +X face (right)
    addFace({ halfX,-halfY, halfZ},{ halfX,-halfY,-halfZ},{ halfX, halfY,-halfZ},{ halfX, halfY, halfZ});
    // -X face (left)
    addFace({-halfX,-halfY,-halfZ},{-halfX,-halfY, halfZ},{-halfX, halfY, halfZ},{-halfX, halfY,-halfZ});

    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(std::make_unique<Mesh>(vertices, indices));
    return std::make_shared<Model>(std::move(meshes),
        glm::vec3(-halfX,-halfY,-halfZ), glm::vec3(halfX,halfY,halfZ));
}

std::shared_ptr<Model> CreateVehicleBody(float halfX, float halfY, float halfZ,
                                          float cabinHeightFraction) {
    // Low-poly car body = lower body slab + raised cabin block.
    //
    // Lower body occupies the full half-extents.
    // Cabin sits on top of the lower body, inset on X and Z, and rises
    // cabinHeightFraction * halfY above the lower body's top.
    //
    // All geometry is flat-shaded (each quad gets its own verts) so that
    // the hard edges between panels read clearly as a stylized low-poly look.

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    auto addFace = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        glm::vec3 tangent = glm::normalize(b - a);
        auto makeV = [&](glm::vec3 p, glm::vec2 uv) {
            Vertex v; v.Position = p; v.Normal = normal;
            v.Tangent = tangent; v.TexCoord = uv;
            return v;
        };
        unsigned int base = static_cast<unsigned int>(vertices.size());
        vertices.push_back(makeV(a, {0,0}));
        vertices.push_back(makeV(b, {1,0}));
        vertices.push_back(makeV(c, {1,1}));
        vertices.push_back(makeV(d, {0,1}));
        indices.insert(indices.end(), {base,base+1,base+2, base,base+2,base+3});
    };

    // --- Lower body (full bounding box, -halfY to +halfY) ---
    const float bY0 = -halfY;
    const float bY1 =  halfY;
    // Front (+Z)
    addFace({-halfX,bY0, halfZ},{halfX,bY0, halfZ},{halfX,bY1, halfZ},{-halfX,bY1, halfZ});
    // Back (-Z)
    addFace({ halfX,bY0,-halfZ},{-halfX,bY0,-halfZ},{-halfX,bY1,-halfZ},{ halfX,bY1,-halfZ});
    // Right (+X)
    addFace({ halfX,bY0, halfZ},{ halfX,bY0,-halfZ},{ halfX,bY1,-halfZ},{ halfX,bY1, halfZ});
    // Left (-X)
    addFace({-halfX,bY0,-halfZ},{-halfX,bY0, halfZ},{-halfX,bY1, halfZ},{-halfX,bY1,-halfZ});
    // Bottom (-Y)
    addFace({-halfX,bY0,-halfZ},{halfX,bY0,-halfZ},{halfX,bY0, halfZ},{-halfX,bY0, halfZ});

    // --- Cabin (inset, sits on top of lower body) ---
    const float cabinInsetX = halfX * 0.65f;
    const float cabinInsetZFront = halfZ * 0.25f;  // cabin ends before front bumper
    const float cabinInsetZBack  = halfZ * 0.30f;  // cabin ends before rear
    // Cabin sits directly on body top (no gap) to avoid visible seams at certain angles
    const float cabinY0 = halfY;
    const float cabinY1 = cabinY0 + halfY * cabinHeightFraction;
    const float cX0 = -cabinInsetX, cX1 = cabinInsetX;
    const float cZ0 = -halfZ + cabinInsetZBack;
    const float cZ1 =  halfZ - cabinInsetZFront;

    // Cabin top
    addFace({cX0,cabinY1,cZ1},{cX1,cabinY1,cZ1},{cX1,cabinY1,cZ0},{cX0,cabinY1,cZ0});
    // Cabin front
    addFace({cX0,cabinY0,cZ1},{cX1,cabinY0,cZ1},{cX1,cabinY1,cZ1},{cX0,cabinY1,cZ1});
    // Cabin back
    addFace({cX1,cabinY0,cZ0},{cX0,cabinY0,cZ0},{cX0,cabinY1,cZ0},{cX1,cabinY1,cZ0});
    // Cabin right
    addFace({cX1,cabinY0,cZ1},{cX1,cabinY0,cZ0},{cX1,cabinY1,cZ0},{cX1,cabinY1,cZ1});
    // Cabin left
    addFace({cX0,cabinY0,cZ0},{cX0,cabinY0,cZ1},{cX0,cabinY1,cZ1},{cX0,cabinY1,cZ0});

    // Lower body top — emitted as 3 pieces (front shoulder, rear shoulder, center strip
    // behind/in-front-of cabin) so there are no gaps around the cabin footprint.
    //
    // These faces sit slightly below cabinY0 to avoid z-fighting with the cabin base,
    // but the offset is minimal (0.005 units) to keep the seam visually unnoticeable.
    constexpr float kShoulderYNudge = 0.005f;
    const float shoulderY = halfY - kShoulderYNudge;

    // Front shoulder (between front bumper and cabin front)
    addFace({-halfX,shoulderY, halfZ},{halfX,shoulderY, halfZ},{halfX,shoulderY,cZ1},{-halfX,shoulderY,cZ1});
    // Rear shoulder
    addFace({-halfX,shoulderY,cZ0},{halfX,shoulderY,cZ0},{halfX,shoulderY,-halfZ},{-halfX,shoulderY,-halfZ});
    // Left gutter (between left side and cabin left)
    addFace({-halfX,shoulderY,cZ1},{cX0,shoulderY,cZ1},{cX0,shoulderY,cZ0},{-halfX,shoulderY,cZ0});
    // Right gutter
    addFace({cX1,shoulderY,cZ1},{halfX,shoulderY,cZ1},{halfX,shoulderY,cZ0},{cX1,shoulderY,cZ0});

    std::vector<std::unique_ptr<Mesh>> meshes;
    meshes.push_back(std::make_unique<Mesh>(vertices, indices));

    // Bounds are exact to the mesh geometry: cabin top is the highest point
    const glm::vec3 boundsMin(-halfX, -halfY, -halfZ);
    const float maxY = halfY + halfY * cabinHeightFraction;
    const glm::vec3 boundsMax(halfX, maxY, halfZ);
    return std::make_shared<Model>(std::move(meshes), boundsMin, boundsMax);
}

} // namespace Primitives
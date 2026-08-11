#include "TerrainSystem.h"
#include "../rendering/Shader.h"
#include <cmath>
#include <limits>

TerrainSystem::TerrainSystem(float chunkSize, int resolution, int loadRadius)
    : m_chunkSize(chunkSize), m_resolution(resolution), m_loadRadius(loadRadius) {}

TerrainSystem::ChunkKey TerrainSystem::WorldToChunk(const glm::vec3& position) const {
    return {
        static_cast<int>(std::floor(position.x / m_chunkSize)),
        static_cast<int>(std::floor(position.z / m_chunkSize))
    };
}

void TerrainSystem::Update(const glm::vec3& viewerPosition) {
    const ChunkKey center = WorldToChunk(viewerPosition);

    std::unordered_set<ChunkKey, ChunkKeyHash> desired;
    for (int dx = -m_loadRadius; dx <= m_loadRadius; ++dx) {
        for (int dz = -m_loadRadius; dz <= m_loadRadius; ++dz) {
            desired.insert({ center.x + dx, center.z + dz });
        }
    }

    for (const auto& key : desired) {
        if (m_chunks.find(key) == m_chunks.end()) {
            m_chunks[key] = std::make_unique<TerrainChunk>(key.x, key.z, m_chunkSize, m_resolution);
        }
    }

    for (auto it = m_chunks.begin(); it != m_chunks.end();) {
        if (desired.find(it->first) == desired.end()) {
            it = m_chunks.erase(it);
        } else {
            ++it;
        }
    }
}

void TerrainSystem::Render(const Shader& shader, const glm::mat4& view, const glm::mat4& projection,
                            const glm::vec3& viewPos, const glm::vec3& dirLightDir, const glm::vec3& dirLightColor) const {
    shader.Bind();
    shader.SetMat4("uView", view);
    shader.SetMat4("uProjection", projection);
    shader.SetMat4("uModel", glm::mat4(1.0f)); // terrain vertices are already in world space
    shader.SetVec3("uViewPos", viewPos);
    shader.SetVec3("uDirLightDirection", dirLightDir);
    shader.SetVec3("uDirLightColor", dirLightColor);
    shader.SetVec3("uAlbedoTint", glm::vec3(0.35f, 0.55f, 0.28f)); // flat grass-green placeholder — Phase C adds real texturing

    for (const auto& [key, chunk] : m_chunks) {
        chunk->Draw(shader);
    }
}

TerrainChunk* TerrainSystem::FindChunkAtWorldXZ(float worldX, float worldZ) {
    const ChunkKey key = WorldToChunk(glm::vec3(worldX, 0.0f, worldZ));
    auto it = m_chunks.find(key);
    return it != m_chunks.end() ? it->second.get() : nullptr;
}

bool TerrainSystem::RaycastTerrain(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                    glm::vec3& outHitPoint, TerrainChunk** outChunk) const {
    // Fixed-step march (coarse), then a short binary-search refine once a
    // sign change (ray crosses from above to below the surface) is found —
    // matches the "raymarch the heightmap" approach noted in the design
    // doc's reference research (§2).
    constexpr float kStepSize = 0.5f;
    constexpr int kMaxSteps = 2000; // covers up to 1000 world units at 0.5 step
    constexpr int kRefineIterations = 8;

    float t = 0.0f;
    glm::vec3 prevPoint = origin;
    bool hadPrevSample = false;
    float prevDiff = 0.0f;

    for (int step = 0; step < kMaxSteps && t < maxDistance; ++step, t += kStepSize) {
        const glm::vec3 point = origin + direction * t;

        TerrainChunk* chunk = nullptr;
        for (const auto& [key, c] : m_chunks) {
            if (c->ContainsWorldXZ(point.x, point.z)) { chunk = c.get(); break; }
        }
        if (!chunk) {
            hadPrevSample = false;
            continue;
        }

        const float terrainHeight = chunk->SampleHeightBilinear(point.x, point.z);
        const float diff = point.y - terrainHeight; // positive = above surface, negative = below

        if (hadPrevSample && prevDiff > 0.0f && diff <= 0.0f) {
            // Crossed the surface between prevPoint and point — binary search refine.
            float tLo = t - kStepSize, tHi = t;
            for (int i = 0; i < kRefineIterations; ++i) {
                const float tMid = (tLo + tHi) * 0.5f;
                const glm::vec3 midPoint = origin + direction * tMid;
                const float midHeight = chunk->SampleHeightBilinear(midPoint.x, midPoint.z);
                if (midPoint.y > midHeight) tLo = tMid; else tHi = tMid;
            }
            outHitPoint = origin + direction * ((tLo + tHi) * 0.5f);
            outHitPoint.y = chunk->SampleHeightBilinear(outHitPoint.x, outHitPoint.z);
            if (outChunk) *outChunk = chunk;
            return true;
        }

        prevPoint = point;
        prevDiff = diff;
        hadPrevSample = true;
    }

    return false;
}

void TerrainSystem::ApplyBrush(TerrainChunk& chunk, const glm::vec3& worldHitPoint, BrushType type,
                                float radius, float strength, float deltaTime) {
    const int resolution = chunk.GetResolution();
    const float chunkSize = chunk.GetChunkSize();
    const float spacing = chunkSize / static_cast<float>(resolution - 1);

    const float localHitX = worldHitPoint.x - chunk.ChunkX() * chunkSize;
    const float localHitZ = worldHitPoint.z - chunk.ChunkZ() * chunkSize;

    const int minIx = std::max(0, static_cast<int>(std::floor((localHitX - radius) / spacing)));
    const int maxIx = std::min(resolution - 1, static_cast<int>(std::ceil((localHitX + radius) / spacing)));
    const int minIz = std::max(0, static_cast<int>(std::floor((localHitZ - radius) / spacing)));
    const int maxIz = std::min(resolution - 1, static_cast<int>(std::ceil((localHitZ + radius) / spacing)));

    const float amount = strength * deltaTime;

    for (int iz = minIz; iz <= maxIz; ++iz) {
        for (int ix = minIx; ix <= maxIx; ++ix) {
            const float sampleX = ix * spacing;
            const float sampleZ = iz * spacing;
            const float dist = glm::length(glm::vec2(sampleX - localHitX, sampleZ - localHitZ));
            if (dist > radius) continue;

            // Linear falloff — smoothstep is a documented fast-follow (design
            // doc §4.1), starting simple here.
            const float falloff = 1.0f - (dist / radius);
            const float currentHeight = chunk.GetHeight(ix, iz);

            switch (type) {
                case BrushType::Raise:
                    chunk.SetHeight(ix, iz, currentHeight + amount * falloff);
                    break;
                case BrushType::Lower:
                    chunk.SetHeight(ix, iz, currentHeight - amount * falloff);
                    break;
                case BrushType::Smooth: {
                    // Average of the 8 neighbors (design doc §2's cited
                    // technique), blended toward by falloff * amount rather
                    // than snapping instantly.
                    float sum = 0.0f;
                    int count = 0;
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dz == 0) continue;
                            sum += chunk.GetHeight(ix + dx, iz + dz);
                            ++count;
                        }
                    }
                    const float average = count > 0 ? sum / static_cast<float>(count) : currentHeight;
                    const float blend = glm::clamp(amount * falloff, 0.0f, 1.0f);
                    chunk.SetHeight(ix, iz, glm::mix(currentHeight, average, blend));
                    break;
                }
            }
        }
    }

    chunk.RebuildMesh();
}
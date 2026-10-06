#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json_fwd.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>

// Session-lifetime record of everything that's diverged from a chunk's
// generated/JSON baseline: destroyed buildings, destroyed or moved
// hand-placed entities, and runtime-spawned extras (e.g. dropped pickups).
// SaveSystem serializes this directly; SceneLoader consults it whenever a
// chunk streams back in so the world reflects prior play.
struct ChunkDelta {
    std::unordered_set<int64_t> DestroyedBuildingPlots;   // packed (plotX,plotZ)
    std::unordered_set<int> DestroyedJsonIndices;
    std::unordered_map<int, std::pair<glm::vec3, glm::quat>> MovedJsonEntities;

    struct SpawnedEntity {
        std::string ModelPath;
        glm::vec3 Position{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
    std::vector<SpawnedEntity> SpawnedEntities;

    bool IsEmpty() const {
        return DestroyedBuildingPlots.empty() && DestroyedJsonIndices.empty()
            && MovedJsonEntities.empty() && SpawnedEntities.empty();
    }
};

class ChunkDeltaStore {
public:
    static ChunkDeltaStore& Get();

    void RecordBuildingDestroyed(int chunkX, int chunkZ, int plotX, int plotZ);
    void RecordJsonEntityDestroyed(int chunkX, int chunkZ, int jsonIndex);
    void RecordJsonEntityMoved(int chunkX, int chunkZ, int jsonIndex, const glm::vec3& pos, const glm::quat& rot);
    void RecordEntitySpawned(int chunkX, int chunkZ, const std::string& modelPath, const glm::vec3& pos, const glm::quat& rot);

    // Returns nullptr if this chunk has no recorded deltas.
    const ChunkDelta* GetDelta(int chunkX, int chunkZ) const;

    void Clear();

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);

    static int64_t PackCoord(int a, int b) {
        return (static_cast<int64_t>(a) << 32) | static_cast<uint32_t>(b);
    }

private:
    struct ChunkKey {
        int x, z;
        bool operator==(const ChunkKey& other) const { return x == other.x && z == other.z; }
    };
    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& k) const {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 1);
        }
    };

    std::unordered_map<ChunkKey, ChunkDelta, ChunkKeyHash> m_deltas;
};
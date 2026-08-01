#include "ChunkDeltaStore.h"
#include <nlohmann/json.hpp>

ChunkDeltaStore& ChunkDeltaStore::Get() {
    static ChunkDeltaStore instance;
    return instance;
}

void ChunkDeltaStore::RecordBuildingDestroyed(int chunkX, int chunkZ, int plotX, int plotZ) {
    m_deltas[{chunkX, chunkZ}].DestroyedBuildingPlots.insert(PackCoord(plotX, plotZ));
}

void ChunkDeltaStore::RecordJsonEntityDestroyed(int chunkX, int chunkZ, int jsonIndex) {
    auto& delta = m_deltas[{chunkX, chunkZ}];
    delta.DestroyedJsonIndices.insert(jsonIndex);
    delta.MovedJsonEntities.erase(jsonIndex); // destroyed takes priority over a stale move
}

void ChunkDeltaStore::RecordJsonEntityMoved(int chunkX, int chunkZ, int jsonIndex, const glm::vec3& pos, const glm::quat& rot) {
    auto& delta = m_deltas[{chunkX, chunkZ}];
    if (delta.DestroyedJsonIndices.count(jsonIndex)) return; // already destroyed, ignore
    delta.MovedJsonEntities[jsonIndex] = { pos, rot };
}

void ChunkDeltaStore::RecordEntitySpawned(int chunkX, int chunkZ, const std::string& modelPath, const glm::vec3& pos, const glm::quat& rot) {
    m_deltas[{chunkX, chunkZ}].SpawnedEntities.push_back({ modelPath, pos, rot });
}

const ChunkDelta* ChunkDeltaStore::GetDelta(int chunkX, int chunkZ) const {
    auto it = m_deltas.find({chunkX, chunkZ});
    return it != m_deltas.end() ? &it->second : nullptr;
}

void ChunkDeltaStore::Clear() {
    m_deltas.clear();
}

nlohmann::json ChunkDeltaStore::ToJson() const {
    nlohmann::json out = nlohmann::json::object();
    for (const auto& [key, delta] : m_deltas) {
        if (delta.IsEmpty()) continue;
        std::string chunkKeyStr = std::to_string(key.x) + "_" + std::to_string(key.z);

        nlohmann::json entry;
        entry["destroyedBuildings"] = std::vector<int64_t>(delta.DestroyedBuildingPlots.begin(), delta.DestroyedBuildingPlots.end());
        entry["destroyedJsonIndices"] = std::vector<int>(delta.DestroyedJsonIndices.begin(), delta.DestroyedJsonIndices.end());

        nlohmann::json moved = nlohmann::json::array();
        for (const auto& [index, transform] : delta.MovedJsonEntities) {
            const auto& [pos, rot] = transform;
            moved.push_back({
                {"index", index},
                {"position", {pos.x, pos.y, pos.z}},
                {"rotation", {rot.w, rot.x, rot.y, rot.z}}
            });
        }
        entry["movedJsonEntities"] = moved;

        nlohmann::json spawned = nlohmann::json::array();
        for (const auto& s : delta.SpawnedEntities) {
            spawned.push_back({
                {"model", s.ModelPath},
                {"position", {s.Position.x, s.Position.y, s.Position.z}},
                {"rotation", {s.Rotation.w, s.Rotation.x, s.Rotation.y, s.Rotation.z}}
            });
        }
        entry["spawnedEntities"] = spawned;

        out[chunkKeyStr] = entry;
    }
    return out;
}

void ChunkDeltaStore::FromJson(const nlohmann::json& j) {
    m_deltas.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& keyStr = it.key();
        size_t underscorePos = keyStr.find('_');
        if (underscorePos == std::string::npos) continue;

        ChunkKey key{
            std::stoi(keyStr.substr(0, underscorePos)),
            std::stoi(keyStr.substr(underscorePos + 1))
        };

        ChunkDelta delta;
        const auto& entry = it.value();

        if (entry.contains("destroyedBuildings")) {
            for (int64_t packed : entry["destroyedBuildings"]) {
                delta.DestroyedBuildingPlots.insert(packed);
            }
        }
        if (entry.contains("destroyedJsonIndices")) {
            for (int idx : entry["destroyedJsonIndices"]) {
                delta.DestroyedJsonIndices.insert(idx);
            }
        }
        if (entry.contains("movedJsonEntities")) {
            for (const auto& m : entry["movedJsonEntities"]) {
                glm::vec3 pos(m["position"][0], m["position"][1], m["position"][2]);
                glm::quat rot(m["rotation"][0], m["rotation"][1], m["rotation"][2], m["rotation"][3]);
                delta.MovedJsonEntities[m["index"].get<int>()] = { pos, rot };
            }
        }
        if (entry.contains("spawnedEntities")) {
            for (const auto& s : entry["spawnedEntities"]) {
                glm::vec3 pos(s["position"][0], s["position"][1], s["position"][2]);
                glm::quat rot(s["rotation"][0], s["rotation"][1], s["rotation"][2], s["rotation"][3]);
                delta.SpawnedEntities.push_back({ s["model"].get<std::string>(), pos, rot });
            }
        }

        m_deltas[key] = std::move(delta);
    }
}
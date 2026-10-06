#include "SaveSystem.h"
#include "ChunkDeltaStore.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include "../core/AssetPaths.h"

namespace {
std::string SavesDir() {
    return AssetPaths::Root(AssetPaths::Category::Saves);
}

nlohmann::json Vec3ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
nlohmann::json QuatToJson(const glm::quat& q) { return { q.w, q.x, q.y, q.z }; }
glm::vec3 JsonToVec3(const nlohmann::json& j) { return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>()); }
glm::quat JsonToQuat(const nlohmann::json& j) { return glm::quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()); }

nlohmann::json TransformToJson(const SavedTransform& t) {
    return {
        {"position", Vec3ToJson(t.Position)},
        {"rotation", QuatToJson(t.Rotation)}
    };
}

SavedTransform TransformFromJson(const nlohmann::json& j) {
    SavedTransform t;
    t.Position = JsonToVec3(j["position"]);
    t.Rotation = JsonToQuat(j["rotation"]);
    return t;
}
} // namespace

std::string SaveSystem::SanitizeSlotName(const std::string& slotName) {
    // Strip anything that isn't a path-safe character — prevents a typed
    // slot name like "../../evil" from escaping the saves/ directory, and
    // keeps the resulting filename valid on Windows.
    std::string clean;
    clean.reserve(slotName.size());
    for (char c : slotName) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ' ') {
            clean += c;
        }
    }
    return clean.empty() ? "save" : clean;
}

std::string SaveSystem::GetSaveFilePath(const std::string& slotName) {
    return SavesDir() + "/" + SanitizeSlotName(slotName) + ".json";
}

std::vector<std::string> SaveSystem::ListSaveSlots() {
    std::vector<std::string> slots;
    std::filesystem::path dir(SavesDir());
    if (!std::filesystem::exists(dir)) {
        return slots;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            slots.push_back(entry.path().stem().string());
        }
    }
    return slots;
}

bool SaveSystem::DeleteSaveSlot(const std::string& slotName) {
    std::error_code error;
    const bool removed = std::filesystem::remove(GetSaveFilePath(slotName), error);
    if (error) {
        Log::Warn("SaveSystem: failed to delete save slot '{}': {}", slotName, error.message());
        return false;
    }
    if (removed) {
        Log::Info("SaveSystem: deleted save slot '{}'", slotName);
    }
    return removed;
}

bool SaveSystem::WriteSaveFile(const std::string& slotName, const SaveGameData& data) {
    if (slotName.empty()) {
        Log::Warn("SaveSystem: refused to save with an empty slot name");
        return false;
    }

    std::filesystem::create_directories(SavesDir());

    nlohmann::json out;

    out["player"] = {
        {"transform", TransformToJson(data.Player.PlayerTransform)},
        {"health", data.Player.Health},
        {"maxHealth", data.Player.MaxHealth},
        {"ammoCurrent", data.Player.AmmoCurrent},
        {"ammoReserve", data.Player.AmmoReserve},
        {"insideVehicle", data.Player.InsideVehicle},
        {"activeVehicleIndex", data.Player.ActiveVehicleIndex}
    };

    nlohmann::json vehiclesJson = nlohmann::json::array();
    for (const auto& vehicle : data.Vehicles) {
        vehiclesJson.push_back({
            {"transform", TransformToJson(vehicle.ChassisTransform)}
        });
    }
    out["vehicles"] = vehiclesJson;

    // Fold in the ChunkDeltaStore singleton's current contents (destroyed
    // buildings, moved/destroyed JSON entities, runtime spawns) so a single
    // save file captures both player/vehicle state and world state.
    out["chunkDeltas"] = ChunkDeltaStore::Get().ToJson();

    const std::string path = GetSaveFilePath(slotName);
    std::ofstream file(path);
    if (!file.is_open()) {
        Log::Error("SaveSystem: failed to open save file for writing: {}", path);
        return false;
    }
    file << out.dump(2);

    Log::Info("SaveSystem: wrote save file for slot '{}'", slotName);
    return true;
}

bool SaveSystem::ReadSaveFile(const std::string& slotName, SaveGameData& outData) {
    const std::string path = GetSaveFilePath(slotName);
    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Error("SaveSystem: save slot not found: {}", slotName);
        return false;
    }

    nlohmann::json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        Log::Error("SaveSystem: failed to parse save file '{}': {}", slotName, e.what());
        return false;
    }

    outData = SaveGameData{};

    if (data.contains("player")) {
        const auto& p = data["player"];
        if (p.contains("transform")) {
            outData.Player.PlayerTransform = TransformFromJson(p["transform"]);
        }
        outData.Player.Health = p.value("health", 100.0f);
        outData.Player.MaxHealth = p.value("maxHealth", 100.0f);
        outData.Player.AmmoCurrent = p.value("ammoCurrent", 0);
        outData.Player.AmmoReserve = p.value("ammoReserve", 0);
        outData.Player.InsideVehicle = p.value("insideVehicle", false);
        outData.Player.ActiveVehicleIndex = p.value("activeVehicleIndex", -1);
    }

    if (data.contains("vehicles")) {
        for (const auto& v : data["vehicles"]) {
            SavedVehicleState vs;
            if (v.contains("transform")) {
                vs.ChassisTransform = TransformFromJson(v["transform"]);
            }
            outData.Vehicles.push_back(vs);
        }
    }

    if (data.contains("chunkDeltas")) {
        ChunkDeltaStore::Get().FromJson(data["chunkDeltas"]);
    } else {
        ChunkDeltaStore::Get().Clear();
    }

    Log::Info("SaveSystem: read save file for slot '{}' ({} vehicles)", slotName, outData.Vehicles.size());
    return true;
}
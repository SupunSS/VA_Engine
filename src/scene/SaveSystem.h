#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

struct SavedTransform {
    glm::vec3 Position{0.0f};
    glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct SavedPlayerState {
    SavedTransform PlayerTransform;
    float Health = 100.0f;
    float MaxHealth = 100.0f;
    int AmmoCurrent = 0;
    int AmmoReserve = 0;
    bool InsideVehicle = false;
    int ActiveVehicleIndex = -1; // index into SaveGameData::Vehicles, -1 if none
};

struct SavedVehicleState {
    SavedTransform ChassisTransform;
};

struct SaveGameData {
    SavedPlayerState Player;
    std::vector<SavedVehicleState> Vehicles;
};

// Pure data I/O — writes/reads plain save data plus the ChunkDeltaStore
// singleton's contents (destroyed buildings, moved/destroyed props, runtime
// spawns) into the same file. Deliberately has no knowledge of ECS/Jolt —
// main.cpp owns translating Scene/PhysicsWorld state into SaveGameData and
// back, the same separation ResetToInitialState already uses.
class SaveSystem {
public:
    static bool WriteSaveFile(const std::string& slotName, const SaveGameData& data);
    static bool ReadSaveFile(const std::string& slotName, SaveGameData& outData);

    static std::vector<std::string> ListSaveSlots();
    static bool DeleteSaveSlot(const std::string& slotName);

private:
    static std::string GetSaveFilePath(const std::string& slotName);
    static std::string SanitizeSlotName(const std::string& slotName);
};
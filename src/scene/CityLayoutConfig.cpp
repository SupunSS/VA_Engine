#include "CityLayoutConfig.h"
#include "../core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>

CityLayoutConfig CityLayoutConfig::s_instance;

const CityLayoutConfig& CityLayoutConfig::Get() {
    return s_instance;
}

void CityLayoutConfig::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("City layout config not found at {} — using built-in defaults", path);
        return;
    }

    nlohmann::json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        Log::Warn("Failed to parse city layout config {}: {} — using built-in defaults", path, e.what());
        return;
    }

    // Start from the compiled-in defaults and only overwrite fields the
    // JSON actually specifies — a partial config file (e.g. just
    // "roadWidth") is valid and leaves everything else untouched.
    CityLayoutConfig config;

    if (data.contains("roadWidth"))            config.RoadWidth = data["roadWidth"].get<float>();
    if (data.contains("sidewalkWidth"))         config.SidewalkWidth = data["sidewalkWidth"].get<float>();
    if (data.contains("buildingGridSize"))      config.BuildingGridSize = data["buildingGridSize"].get<int>();
    if (data.contains("buildingMargin"))        config.BuildingMargin = data["buildingMargin"].get<float>();
    if (data.contains("buildingMinHeight"))     config.BuildingMinHeight = data["buildingMinHeight"].get<float>();
    if (data.contains("buildingMaxHeight"))     config.BuildingMaxHeight = data["buildingMaxHeight"].get<float>();
    if (data.contains("groundThickness"))       config.GroundThickness = data["groundThickness"].get<float>();
    if (data.contains("pedestrianWalkHeight"))  config.PedestrianWalkHeight = data["pedestrianWalkHeight"].get<float>();
    if (data.contains("pedestrianMoveSpeed"))   config.PedestrianMoveSpeed = data["pedestrianMoveSpeed"].get<float>();
    if (data.contains("pedestriansPerChunk"))   config.PedestriansPerChunk = data["pedestriansPerChunk"].get<int>();
    if (data.contains("waypointsPerEdge"))      config.WaypointsPerEdge = data["waypointsPerEdge"].get<int>();

    s_instance = config;
    Log::Info("City layout config loaded from {}", path);
}
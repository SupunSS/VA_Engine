#pragma once
#include <string>

// Single, load-once source of truth for the tunable numbers behind
// procedural city generation (SceneLoader) and NPC pathing (CityLayout).
// Values come from a JSON file loaded once at startup; any field missing
// from the file — or the file itself being missing — keeps that field's
// default below, so introducing this system never changes existing
// behavior until you actually edit config/city_layout.json.
struct CityLayoutConfig {
    float RoadWidth = 8.0f;
    float SidewalkWidth = 2.0f;

    int   BuildingGridSize = 2;      // buildings per chunk edge (2 = 2x2 = 4/chunk)
    float BuildingMargin = 1.0f;     // gap between a building's footprint and its plot edge
    float BuildingMinHeight = 6.0f;
    float BuildingMaxHeight = 24.0f;

    float GroundThickness = 10.0f;

    float PedestrianWalkHeight = 0.15f;
    float PedestrianMoveSpeed = 1.4f; // m/s
    int   PedestriansPerChunk = 3;
    int   WaypointsPerEdge = 3;       // sidewalk/road loop subdivision

    static const CityLayoutConfig& Get();
    static void LoadFromFile(const std::string& path);

private:
    static CityLayoutConfig s_instance;
};
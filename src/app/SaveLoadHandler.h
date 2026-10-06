#pragma once

struct AppState;

// Draws the Save/Load panel and, if the user pressed Save or Load this frame,
// performs it: writes the player/vehicle state to a slot, or restores it
// (player, health/ammo, chunks, vehicles, occupancy).
void HandleSaveLoadPanel(AppState& app);
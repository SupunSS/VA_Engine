#pragma once

struct AppState;

// Creates the player entity (+ visual child entity), loads its model and
// animations, and builds its animator and animation state machine.
// Requires app.world.scene to exist already. Fills app.player.
void CreatePlayer(AppState& app);
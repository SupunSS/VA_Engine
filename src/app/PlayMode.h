#pragma once

struct AppState;

// Play/stop/reset logic for the editor. All state lives in AppState.
namespace PlayMode {

// Enter play mode: lock the cursor, place the player at spawn, stream chunks around it.
void Start(AppState& app);

// Leave play mode but keep the world as it is.
void StopKeepState(AppState& app);

// Destroy every spawned vehicle (engine audio source, wheels, chassis).
void DestroyAllVehicles(AppState& app);

// Put the player, camera, animation and selection back to their initial state
// and remove vehicles and physics test bodies.
void ResetToInitialState(AppState& app);

// Called when a project is created/opened: reset, then replace the city with
// a blank flat world (no streaming, no pedestrians, one invisible floor).
void ResetWorldForProject(AppState& app);

} // namespace PlayMode
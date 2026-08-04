#pragma once
#include <memory>
#include <string>
#include "AnimationStateMachine.h"

class Model;

namespace AnimationStateMachineLoader {

// Loads a state machine definition from JSON and resolves each state's
// "clip" field via model->LoadAnimation(...) — the same model the entity
// visually uses, since an animation clip only makes sense against the
// skeleton it was authored for.
//
// Returns nullptr (and logs a clear reason) on any failure: file not
// found, malformed JSON, a state referencing a clip that fails to load,
// or a transition referencing an unknown state. Never partially
// constructs and returns a broken machine.
std::shared_ptr<AnimationStateMachine> LoadFromFile(const std::string& filePath, Model& model);

} // namespace AnimationStateMachineLoader
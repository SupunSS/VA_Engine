#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "AnimationStateMachine.h"

class Model;

namespace AnimationStateMachineLoader {

// Optional editor-only node layout, round-tripped through an optional
// "layout" section in the JSON. Purely cosmetic — never affects runtime
// behavior, only where the Animator Editor draws each state's box.
struct StateMachineLayout {
    struct Vec2 { float x = 0.0f; float y = 0.0f; };
    std::unordered_map<std::string, Vec2> NodePositions;
};

std::shared_ptr<AnimationStateMachine> LoadFromFile(const std::string& filePath, Model& model,
                                                     StateMachineLayout* outLayout = nullptr);

// Writes the machine back out in the same format LoadFromFile reads.
// filePath should be a resolved, absolute-or-project-relative path (the
// caller is responsible for resolving through AssetPaths first, same as
// LoadFromFile's caller does). Returns false and logs a reason on failure.
bool SaveToFile(const AnimationStateMachine& machine, const std::string& filePath,
                 const StateMachineLayout* layout = nullptr);

} // namespace AnimationStateMachineLoader
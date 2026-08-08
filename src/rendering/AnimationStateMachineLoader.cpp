#include "AnimationStateMachineLoader.h"
#include "Model.h"
#include "../core/Log.h"
#include "../core/AssetPaths.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_set>

namespace AnimationStateMachineLoader {

namespace {

using json = nlohmann::json;

bool ParseParamType(const std::string& typeStr, AnimationStateMachine::ParamType& outType) {
    if (typeStr == "bool")  { outType = AnimationStateMachine::ParamType::Bool;  return true; }
    if (typeStr == "float") { outType = AnimationStateMachine::ParamType::Float; return true; }
    if (typeStr == "int")   { outType = AnimationStateMachine::ParamType::Int;   return true; }
    return false;
}

bool ParseComparison(const std::string& compStr, AnimationStateMachine::Comparison& outComp) {
    if (compStr == "equals")           { outComp = AnimationStateMachine::Comparison::Equals;         return true; }
    if (compStr == "notEquals")        { outComp = AnimationStateMachine::Comparison::NotEquals;      return true; }
    if (compStr == "greaterThan")      { outComp = AnimationStateMachine::Comparison::GreaterThan;    return true; }
    if (compStr == "lessThan")         { outComp = AnimationStateMachine::Comparison::LessThan;       return true; }
    if (compStr == "greaterOrEqual")   { outComp = AnimationStateMachine::Comparison::GreaterOrEqual; return true; }
    if (compStr == "lessOrEqual")      { outComp = AnimationStateMachine::Comparison::LessOrEqual;    return true; }
    return false;
}

const char* ParamTypeToString(AnimationStateMachine::ParamType type) {
    switch (type) {
        case AnimationStateMachine::ParamType::Bool:  return "bool";
        case AnimationStateMachine::ParamType::Float: return "float";
        case AnimationStateMachine::ParamType::Int:   return "int";
    }
    return "bool";
}

const char* ComparisonToString(AnimationStateMachine::Comparison comp) {
    switch (comp) {
        case AnimationStateMachine::Comparison::Equals:         return "equals";
        case AnimationStateMachine::Comparison::NotEquals:      return "notEquals";
        case AnimationStateMachine::Comparison::GreaterThan:    return "greaterThan";
        case AnimationStateMachine::Comparison::LessThan:       return "lessThan";
        case AnimationStateMachine::Comparison::GreaterOrEqual: return "greaterOrEqual";
        case AnimationStateMachine::Comparison::LessOrEqual:    return "lessOrEqual";
    }
    return "equals";
}

} // namespace

std::shared_ptr<AnimationStateMachine> LoadFromFile(const std::string& filePath, Model& model,
                                                     StateMachineLayout* outLayout) {
    const std::string resolvedPath = AssetPaths::Resolve(AssetPaths::Category::AnimStateMachines, filePath);

    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        Log::Error("AnimationStateMachine: failed to open '{}'", resolvedPath);
        return nullptr;
    }

    json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        Log::Error("AnimationStateMachine: failed to parse '{}': {}", resolvedPath, e.what());
        return nullptr;
    }

    if (!data.contains("states") || !data["states"].is_array() || data["states"].empty()) {
        Log::Error("AnimationStateMachine: '{}' has no 'states' array", resolvedPath);
        return nullptr;
    }

    auto machine = std::make_shared<AnimationStateMachine>();
    std::unordered_set<std::string> stateNames;

    for (const auto& stateEntry : data["states"]) {
        const std::string name = stateEntry.value("name", "");
        const std::string clipPath = stateEntry.value("clip", "");
        const float blendIn = stateEntry.value("blendIn", 0.2f);

        if (name.empty() || clipPath.empty()) {
            Log::Error("AnimationStateMachine: '{}' has a state missing 'name' or 'clip'", resolvedPath);
            return nullptr;
        }

        auto clip = model.LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, clipPath));
        if (!clip) {
            Log::Error("AnimationStateMachine: '{}' state '{}' failed to load clip '{}'",
                       resolvedPath, name, clipPath);
            return nullptr;
        }

        machine->AddState(name, clip, blendIn, clipPath);
        stateNames.insert(name);
    }

    const std::string initialState = data.value("initialState", "");
    if (initialState.empty() || !stateNames.count(initialState)) {
        Log::Error("AnimationStateMachine: '{}' has missing or unknown 'initialState'", resolvedPath);
        return nullptr;
    }
    machine->SetInitialState(initialState);

    if (data.contains("transitions")) {
        for (const auto& transitionEntry : data["transitions"]) {
            const std::string from = transitionEntry.value("from", "");
            const std::string to = transitionEntry.value("to", "");

            if (from.empty() || to.empty() || !stateNames.count(to) || (from != "*" && !stateNames.count(from))) {
                Log::Error("AnimationStateMachine: '{}' has a transition with invalid 'from'/'to'", resolvedPath);
                return nullptr;
            }

            std::vector<AnimationStateMachine::Condition> conditions;
            if (transitionEntry.contains("conditions")) {
                for (const auto& condEntry : transitionEntry["conditions"]) {
                    AnimationStateMachine::Condition condition;
                    condition.ParamName = condEntry.value("param", "");

                    const std::string typeStr = condEntry.value("type", "bool");
                    if (!ParseParamType(typeStr, condition.Type)) {
                        Log::Error("AnimationStateMachine: '{}' has unknown condition type '{}'", resolvedPath, typeStr);
                        return nullptr;
                    }

                    const std::string compStr = condEntry.value("comp", "equals");
                    if (!ParseComparison(compStr, condition.Comp)) {
                        Log::Error("AnimationStateMachine: '{}' has unknown comparison '{}'", resolvedPath, compStr);
                        return nullptr;
                    }

                    if (condition.ParamName.empty()) {
                        Log::Error("AnimationStateMachine: '{}' has a condition missing 'param'", resolvedPath);
                        return nullptr;
                    }

                    switch (condition.Type) {
                        case AnimationStateMachine::ParamType::Bool:
                            condition.BoolValue = condEntry.value("value", false);
                            break;
                        case AnimationStateMachine::ParamType::Float:
                            condition.FloatValue = condEntry.value("value", 0.0f);
                            break;
                        case AnimationStateMachine::ParamType::Int:
                            condition.IntValue = condEntry.value("value", 0);
                            break;
                    }

                    conditions.push_back(condition);
                }
            }

            machine->AddTransition(from, to, std::move(conditions));
        }
    }

    machine->SetSourceFilePath(resolvedPath);

    if (outLayout && data.contains("layout")) {
        for (auto& [name, posJson] : data["layout"].items()) {
            StateMachineLayout::Vec2 pos;
            pos.x = posJson.value("x", 0.0f);
            pos.y = posJson.value("y", 0.0f);
            outLayout->NodePositions[name] = pos;
        }
    }

    Log::Info("AnimationStateMachine: loaded '{}' from '{}'", data.value("name", resolvedPath), resolvedPath);
    return machine;
}

bool SaveToFile(const AnimationStateMachine& machine, const std::string& filePath,
                const StateMachineLayout* layout) {
    json data;
    data["name"] = std::filesystem::path(filePath).stem().string();
    data["initialState"] = machine.GetInitialState();

    json statesJson = json::array();
    for (const auto& [name, stateDef] : machine.GetStates()) {
        json stateJson;
        stateJson["name"] = name;
        stateJson["clip"] = stateDef.ClipPath;
        stateJson["blendIn"] = stateDef.BlendInDuration;
        statesJson.push_back(stateJson);
    }
    data["states"] = statesJson;

    json transitionsJson = json::array();
    for (const auto& transition : machine.GetTransitions()) {
        json transitionJson;
        transitionJson["from"] = transition.FromState;
        transitionJson["to"] = transition.ToState;

        json conditionsJson = json::array();
        for (const auto& condition : transition.Conditions) {
            json conditionJson;
            conditionJson["param"] = condition.ParamName;
            conditionJson["type"] = ParamTypeToString(condition.Type);
            conditionJson["comp"] = ComparisonToString(condition.Comp);
            switch (condition.Type) {
                case AnimationStateMachine::ParamType::Bool:  conditionJson["value"] = condition.BoolValue;  break;
                case AnimationStateMachine::ParamType::Float: conditionJson["value"] = condition.FloatValue; break;
                case AnimationStateMachine::ParamType::Int:   conditionJson["value"] = condition.IntValue;   break;
            }
            conditionsJson.push_back(conditionJson);
        }
        transitionJson["conditions"] = conditionsJson;
        transitionsJson.push_back(transitionJson);
    }
    data["transitions"] = transitionsJson;

    if (layout) {
        json layoutJson;
        for (const auto& [name, pos] : layout->NodePositions) {
            layoutJson[name] = { {"x", pos.x}, {"y", pos.y} };
        }
        data["layout"] = layoutJson;
    }

    std::ofstream file(filePath);
    if (!file.is_open()) {
        Log::Error("AnimationStateMachine: failed to open '{}' for writing", filePath);
        return false;
    }
    file << data.dump(2);
    Log::Info("AnimationStateMachine: saved '{}'", filePath);
    return true;
}

} // namespace AnimationStateMachineLoader
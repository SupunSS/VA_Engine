#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Animation.h"
#include "Animator.h"

class AnimationStateMachine {
public:
    enum class Comparison {
        Equals, NotEquals, GreaterThan, LessThan, GreaterOrEqual, LessOrEqual
    };

    enum class ParamType {
        Bool, Float, Int
    };

    struct Condition {
        std::string ParamName;
        ParamType Type = ParamType::Bool;
        Comparison Comp = Comparison::Equals;
        bool BoolValue = false;
        float FloatValue = 0.0f;
        int IntValue = 0;
    };

    struct StateDef {
        std::shared_ptr<Animation> Clip;
        float BlendInDuration = 0.2f;
        // Raw, unresolved path as authored (e.g. "characters/idle.fbx") —
        // this is what gets written back out by AnimationStateMachineLoader::SaveToFile.
        // NOT the same as Animation::GetSourcePath(), which stores the resolved path.
        std::string ClipPath;
    };

    struct TransitionDef {
        std::string FromState;
        std::string ToState;
        std::vector<Condition> Conditions;
    };

    void AddState(const std::string& name, std::shared_ptr<Animation> clip,
                  float blendInDuration = 0.2f, const std::string& clipPath = "");
    bool RemoveState(const std::string& name);
    bool RenameState(const std::string& oldName, const std::string& newName);
    void SetStateBlendIn(const std::string& name, float blendIn);
    void SetStateClip(const std::string& name, std::shared_ptr<Animation> clip, const std::string& clipPath);

    void AddTransition(const std::string& fromState, const std::string& toState,
                        std::vector<Condition> conditions);
    void RemoveTransition(size_t index);

    void SetInitialState(const std::string& name);
    const std::string& GetInitialState() const { return m_InitialState; }

    void SetBool(const std::string& name, bool value);
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);

    bool GetBool(const std::string& name) const;
    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;

    void Update(Animator& animator);

    const std::string& GetCurrentState() const { return m_CurrentState; }

    // Introspection / editor accessors
    const std::unordered_map<std::string, StateDef>& GetStates() const { return m_States; }
    std::unordered_map<std::string, StateDef>& GetStatesMutable() { return m_States; }
    const std::vector<TransitionDef>& GetTransitions() const { return m_Transitions; }
    std::vector<TransitionDef>& GetTransitionsMutable() { return m_Transitions; }
    const std::unordered_map<std::string, bool>& GetBoolParams() const { return m_BoolParams; }
    const std::unordered_map<std::string, float>& GetFloatParams() const { return m_FloatParams; }
    const std::unordered_map<std::string, int>& GetIntParams() const { return m_IntParams; }

    // Set by AnimationStateMachineLoader after a successful load — lets the
    // editor's "Save" button write back to the same file without asking.
    void SetSourceFilePath(const std::string& path) { m_SourceFilePath = path; }
    const std::string& GetSourceFilePath() const { return m_SourceFilePath; }

private:
    bool EvaluateCondition(const Condition& condition) const;
    bool AllConditionsPass(const TransitionDef& transition) const;

    std::unordered_map<std::string, StateDef> m_States;
    std::vector<TransitionDef> m_Transitions;
    std::string m_CurrentState;
    std::string m_InitialState;
    bool m_HasEnteredInitialState = false;
    std::string m_SourceFilePath;

    std::unordered_map<std::string, bool> m_BoolParams;
    std::unordered_map<std::string, float> m_FloatParams;
    std::unordered_map<std::string, int> m_IntParams;
};
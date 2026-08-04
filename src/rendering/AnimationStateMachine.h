#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Animation.h"
#include "Animator.h"

// Reusable animation state machine: named states (each backed by a clip and
// a blend-in duration), and named transitions between them gated by
// conditions over a small parameter bag (bools/floats/ints by name —
// e.g. "IsMoving", "Speed"). Call the Set*/Get* accessors each frame to
// update parameters from gameplay code, then Update() evaluates whichever
// transitions apply and drives the underlying Animator automatically,
// including that transition's blend duration.
//
// This class only knows about states/transitions/parameters — it has no
// opinion on how it gets constructed. See AnimationStateMachineLoader for
// building one from a JSON file instead of by hand in C++.
class AnimationStateMachine {
public:
    enum class Comparison {
        Equals,
        NotEquals,
        GreaterThan,
        LessThan,
        GreaterOrEqual,
        LessOrEqual
    };

    enum class ParamType {
        Bool,
        Float,
        Int
    };

    struct Condition {
        std::string ParamName;
        ParamType Type = ParamType::Bool;
        Comparison Comp = Comparison::Equals;
        bool BoolValue = false;
        float FloatValue = 0.0f;
        int IntValue = 0;
    };

    // Adds a state. blendInDuration is how long PlayAnimation() blends INTO
    // this state when a transition targets it (0 = instant snap).
    void AddState(const std::string& name, std::shared_ptr<Animation> clip, float blendInDuration = 0.2f);

    // fromState may be "*" to mean "from any state". All conditions must
    // pass (AND) for the transition to fire. Transitions are evaluated in
    // the order they were added; the first one whose conditions all pass
    // wins for this frame.
    void AddTransition(const std::string& fromState, const std::string& toState,
                        std::vector<Condition> conditions);

    // Sets which state playback starts in. Does not itself call
    // PlayAnimation — call Update() once after this (or rely on the first
    // real Update() call) to actually start playback.
    void SetInitialState(const std::string& name);

    void SetBool(const std::string& name, bool value);
    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);

    bool GetBool(const std::string& name) const;
    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;

    // Evaluates transitions out of the current state and, if one fires,
    // switches the given Animator to the new state's clip with that
    // state's blend-in duration. Call once per frame.
    void Update(Animator& animator);

    const std::string& GetCurrentState() const { return m_CurrentState; }

private:
    struct StateDef {
        std::shared_ptr<Animation> Clip;
        float BlendInDuration = 0.2f;
    };

    struct TransitionDef {
        std::string FromState;
        std::string ToState;
        std::vector<Condition> Conditions;
    };

    bool EvaluateCondition(const Condition& condition) const;
    bool AllConditionsPass(const TransitionDef& transition) const;

    std::unordered_map<std::string, StateDef> m_States;
    std::vector<TransitionDef> m_Transitions;
    std::string m_CurrentState;
    bool m_HasEnteredInitialState = false;

    std::unordered_map<std::string, bool> m_BoolParams;
    std::unordered_map<std::string, float> m_FloatParams;
    std::unordered_map<std::string, int> m_IntParams;
};
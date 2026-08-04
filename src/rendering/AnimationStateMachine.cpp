#include "AnimationStateMachine.h"
#include "../core/Log.h"

void AnimationStateMachine::AddState(const std::string& name, std::shared_ptr<Animation> clip, float blendInDuration) {
    m_States[name] = StateDef{ clip, blendInDuration };
}

void AnimationStateMachine::AddTransition(const std::string& fromState, const std::string& toState,
                                          std::vector<Condition> conditions) {
    m_Transitions.push_back(TransitionDef{ fromState, toState, std::move(conditions) });
}

void AnimationStateMachine::SetInitialState(const std::string& name) {
    m_CurrentState = name;
    m_HasEnteredInitialState = false;
}

void AnimationStateMachine::SetBool(const std::string& name, bool value) { m_BoolParams[name] = value; }
void AnimationStateMachine::SetFloat(const std::string& name, float value) { m_FloatParams[name] = value; }
void AnimationStateMachine::SetInt(const std::string& name, int value) { m_IntParams[name] = value; }

bool AnimationStateMachine::GetBool(const std::string& name) const {
    auto it = m_BoolParams.find(name);
    return it != m_BoolParams.end() ? it->second : false;
}
float AnimationStateMachine::GetFloat(const std::string& name) const {
    auto it = m_FloatParams.find(name);
    return it != m_FloatParams.end() ? it->second : 0.0f;
}
int AnimationStateMachine::GetInt(const std::string& name) const {
    auto it = m_IntParams.find(name);
    return it != m_IntParams.end() ? it->second : 0;
}

bool AnimationStateMachine::EvaluateCondition(const Condition& condition) const {
    switch (condition.Type) {
        case ParamType::Bool: {
            const bool value = GetBool(condition.ParamName);
            switch (condition.Comp) {
                case Comparison::Equals:    return value == condition.BoolValue;
                case Comparison::NotEquals: return value != condition.BoolValue;
                default: return false; // ordering comparisons don't apply to bool
            }
        }
        case ParamType::Float: {
            const float value = GetFloat(condition.ParamName);
            switch (condition.Comp) {
                case Comparison::Equals:         return value == condition.FloatValue;
                case Comparison::NotEquals:      return value != condition.FloatValue;
                case Comparison::GreaterThan:    return value > condition.FloatValue;
                case Comparison::LessThan:       return value < condition.FloatValue;
                case Comparison::GreaterOrEqual: return value >= condition.FloatValue;
                case Comparison::LessOrEqual:    return value <= condition.FloatValue;
            }
            break;
        }
        case ParamType::Int: {
            const int value = GetInt(condition.ParamName);
            switch (condition.Comp) {
                case Comparison::Equals:         return value == condition.IntValue;
                case Comparison::NotEquals:      return value != condition.IntValue;
                case Comparison::GreaterThan:    return value > condition.IntValue;
                case Comparison::LessThan:       return value < condition.IntValue;
                case Comparison::GreaterOrEqual: return value >= condition.IntValue;
                case Comparison::LessOrEqual:    return value <= condition.IntValue;
            }
            break;
        }
    }
    return false;
}

bool AnimationStateMachine::AllConditionsPass(const TransitionDef& transition) const {
    if (transition.FromState != "*" && transition.FromState != m_CurrentState) {
        return false;
    }
    for (const auto& condition : transition.Conditions) {
        if (!EvaluateCondition(condition)) {
            return false;
        }
    }
    return true;
}

void AnimationStateMachine::Update(Animator& animator) {
    if (!m_HasEnteredInitialState) {
        auto it = m_States.find(m_CurrentState);
        if (it != m_States.end()) {
            animator.PlayAnimation(it->second.Clip, 0.0f); // instant — first pose, nothing to blend from
        } else {
            Log::Warn("AnimationStateMachine: initial state '{}' has no matching AddState() call", m_CurrentState);
        }
        m_HasEnteredInitialState = true;
        return;
    }

    for (const auto& transition : m_Transitions) {
        if (!AllConditionsPass(transition)) {
            continue;
        }

        if (transition.ToState == m_CurrentState) {
            return; // "transition" to the same state — nothing to do
        }

        auto it = m_States.find(transition.ToState);
        if (it == m_States.end()) {
            Log::Warn("AnimationStateMachine: transition targets unknown state '{}'", transition.ToState);
            return;
        }

        m_CurrentState = transition.ToState;
        animator.PlayAnimation(it->second.Clip, it->second.BlendInDuration);
        return; // first passing transition wins this frame
    }
}
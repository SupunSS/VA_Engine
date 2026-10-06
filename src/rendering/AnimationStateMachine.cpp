#include "AnimationStateMachine.h"
#include "../core/Log.h"
#include <algorithm>

void AnimationStateMachine::AddState(const std::string& name, std::shared_ptr<Animation> clip,
                                      float blendInDuration, const std::string& clipPath) {
    m_States[name] = StateDef{ clip, blendInDuration, clipPath };
}

bool AnimationStateMachine::RemoveState(const std::string& name) {
    auto it = m_States.find(name);
    if (it == m_States.end()) return false;
    m_States.erase(it);

    m_Transitions.erase(
        std::remove_if(m_Transitions.begin(), m_Transitions.end(),
            [&](const TransitionDef& t) { return t.FromState == name || t.ToState == name; }),
        m_Transitions.end());

    if (m_CurrentState == name) m_CurrentState.clear();
    if (m_InitialState == name) m_InitialState.clear();
    return true;
}

bool AnimationStateMachine::RenameState(const std::string& oldName, const std::string& newName) {
    if (oldName == newName) return true;
    if (newName.empty() || m_States.count(newName) > 0) return false;

    auto it = m_States.find(oldName);
    if (it == m_States.end()) return false;

    StateDef def = it->second;
    m_States.erase(it);
    m_States[newName] = def;

    for (auto& transition : m_Transitions) {
        if (transition.FromState == oldName) transition.FromState = newName;
        if (transition.ToState == oldName) transition.ToState = newName;
    }
    if (m_CurrentState == oldName) m_CurrentState = newName;
    if (m_InitialState == oldName) m_InitialState = newName;
    return true;
}

void AnimationStateMachine::SetStateBlendIn(const std::string& name, float blendIn) {
    auto it = m_States.find(name);
    if (it != m_States.end()) it->second.BlendInDuration = blendIn;
}

void AnimationStateMachine::SetStateClip(const std::string& name, std::shared_ptr<Animation> clip, const std::string& clipPath) {
    auto it = m_States.find(name);
    if (it != m_States.end()) {
        it->second.Clip = clip;
        it->second.ClipPath = clipPath;
    }
}

void AnimationStateMachine::AddTransition(const std::string& fromState, const std::string& toState,
                                          std::vector<Condition> conditions) {
    m_Transitions.push_back(TransitionDef{ fromState, toState, std::move(conditions) });
}

void AnimationStateMachine::RemoveTransition(size_t index) {
    if (index < m_Transitions.size()) {
        m_Transitions.erase(m_Transitions.begin() + index);
    }
}

void AnimationStateMachine::SetInitialState(const std::string& name) {
    m_InitialState = name;
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
                default: return false;
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
        if (!EvaluateCondition(condition)) return false;
    }
    return true;
}

void AnimationStateMachine::Update(Animator& animator) {
    if (!m_HasEnteredInitialState) {
        auto it = m_States.find(m_CurrentState);
        if (it != m_States.end()) {
            animator.PlayAnimation(it->second.Clip, 0.0f);
        } else {
            Log::Warn("AnimationStateMachine: initial state '{}' has no matching AddState() call", m_CurrentState);
        }
        m_HasEnteredInitialState = true;
        return;
    }

    for (const auto& transition : m_Transitions) {
        if (!AllConditionsPass(transition)) continue;
        if (transition.ToState == m_CurrentState) return;

        auto it = m_States.find(transition.ToState);
        if (it == m_States.end()) {
            Log::Warn("AnimationStateMachine: transition targets unknown state '{}'", transition.ToState);
            return;
        }

        m_CurrentState = transition.ToState;
        animator.PlayAnimation(it->second.Clip, it->second.BlendInDuration);
        return;
    }
}
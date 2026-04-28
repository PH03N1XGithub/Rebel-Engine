#include "Engine/Framework/EnginePch.h"
#include "Engine/Input/InputComponent.h"

#include <cmath>

#include "Engine/Input/PlayerInput.h"

namespace
{
    constexpr float kAxisConsumeEpsilon = 0.001f;
}

void InputComponent::BindAxis(const InputAction action, AxisDelegate func, const bool bConsume)
{
    if (!func.IsBound())
        return;

    m_AxisBindings.Emplace(AxisBinding{action, std::move(func), bConsume});
}

void InputComponent::BindAction(const InputAction action, const InputEventType eventType, ActionDelegate func, const bool bConsume)
{
    if (!func.IsBound())
        return;

    m_ActionBindings.Emplace(ActionBinding{action, eventType, std::move(func), bConsume});
}

InputComponent::AxisDelegate& InputComponent::AddAxisBinding(const InputAction action, const bool bConsume)
{
    AxisBinding& binding = m_AxisBindings.Emplace(AxisBinding{action, {}, bConsume});
    return binding.Func;
}

InputComponent::ActionDelegate& InputComponent::AddActionBinding(const InputAction action, const InputEventType eventType, const bool bConsume)
{
    ActionBinding& binding = m_ActionBindings.Emplace(ActionBinding{action, eventType, {}, bConsume});
    return binding.Func;
}

void InputComponent::ClearBindings()
{
    m_AxisBindings.Clear();
    m_ActionBindings.Clear();
}

bool InputComponent::ProcessBindings(const PlayerInput& playerInput, std::array<bool, kInputActionCount>& consumedActions) const
{
    bool bConsumedAny = false;

    for (const AxisBinding& binding : m_AxisBindings)
    {
        const size_t actionIndex = ToInputActionIndex(binding.Action);
        if (actionIndex >= kInputActionCount)
            continue;

        if (consumedActions[actionIndex])
            continue;

        const float axisValue = playerInput.GetAxis(binding.Action);
        binding.Func.Broadcast(axisValue);

        if (binding.bConsume && std::fabs(axisValue) > kAxisConsumeEpsilon)
        {
            consumedActions[actionIndex] = true;
            bConsumedAny = true;
        }
    }

    for (const ActionBinding& binding : m_ActionBindings)
    {
        const size_t actionIndex = ToInputActionIndex(binding.Action);
        if (actionIndex >= kInputActionCount)
            continue;

        if (consumedActions[actionIndex])
            continue;

        bool bInvoke = false;
        switch (binding.EventType)
        {
        case InputEventType::Pressed:
            bInvoke = playerInput.WasActionPressed(binding.Action);
            break;
        case InputEventType::Released:
            bInvoke = playerInput.WasActionReleased(binding.Action);
            break;
        case InputEventType::Held:
            bInvoke = playerInput.IsActionDown(binding.Action);
            break;
        }

        if (!bInvoke)
            continue;

        binding.Func.Broadcast();

        if (binding.bConsume)
        {
            consumedActions[actionIndex] = true;
            bConsumedAny = true;
        }
    }

    return bConsumedAny;
}


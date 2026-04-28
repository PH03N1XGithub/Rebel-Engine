#pragma once

#include <array>
#include <utility>

#include "Core/Delegate.h"
#include "Core/Containers/TArray.h"
#include "Engine/Input/InputAction.h"

class PlayerInput;

enum class InputEventType : uint8
{
    Pressed,
    Released,
    Held
};

class InputComponent
{
public:
    using AxisDelegate = Rebel::Core::TDelegate<float>;
    using ActionDelegate = Rebel::Core::TDelegate<>;

    struct AxisBinding
    {
        InputAction Action = InputAction::MoveForward;
        AxisDelegate Func{};
        bool bConsume = false;
    };

    struct ActionBinding
    {
        InputAction Action = InputAction::Jump;
        InputEventType EventType = InputEventType::Pressed;
        ActionDelegate Func{};
        bool bConsume = true;
    };

    AxisDelegate& AddAxisBinding(InputAction action, bool bConsume = false);
    ActionDelegate& AddActionBinding(InputAction action, InputEventType eventType, bool bConsume = true);

    void BindAxis(InputAction action, AxisDelegate delegate, bool bConsume = false);
    void BindAction(InputAction action, InputEventType eventType, ActionDelegate delegate, bool bConsume = true);

    template<typename TObject>
    void BindAxisRaw(InputAction action, TObject* object, void(TObject::*method)(float), bool bConsume = false)
    {
        AxisDelegate delegate;
        delegate.BindRaw(object, method);
        BindAxis(action, std::move(delegate), bConsume);
    }

    template<typename TObject>
    void BindActionRaw(InputAction action, InputEventType eventType, TObject* object, void(TObject::*method)(), bool bConsume = true)
    {
        ActionDelegate delegate;
        delegate.BindRaw(object, method);
        BindAction(action, eventType, std::move(delegate), bConsume);
    }

    void ClearBindings();

    bool ProcessBindings(const PlayerInput& playerInput, std::array<bool, kInputActionCount>& consumedActions) const;

private:
    TArray<AxisBinding, 16> m_AxisBindings;
    TArray<ActionBinding, 16> m_ActionBindings;
};


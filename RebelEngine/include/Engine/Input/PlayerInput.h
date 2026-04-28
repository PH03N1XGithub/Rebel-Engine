#pragma once

#include <array>

#include "Core/Containers/TArray.h"
#include "Engine/Input/InputAction.h"
#include "Engine/Input/InputFrame.h"

class PlayerInput
{
public:
    PlayerInput();

    void ResetMappings();
    void AddAxisMapping(const InputAxisMapping& mapping);
    void AddActionMapping(const InputActionMapping& mapping);

    void EvaluateFrame(uint64 frameId, double timestampSeconds);

    float GetAxis(InputAction action) const;
    bool IsActionDown(InputAction action) const;
    bool WasActionPressed(InputAction action) const;
    bool WasActionReleased(InputAction action) const;
    const ActionState& GetActionState(InputAction action) const;

    const InputFrame& GetFrame() const { return m_Frame; }
    void SetDebugLoggingEnabled(bool bEnabled) { m_bDebugLoggingEnabled = bEnabled; }
    bool IsDebugLoggingEnabled() const { return m_bDebugLoggingEnabled; }

private:
    void evaluateKeys(uint64 frameId, double timestampSeconds);
    void evaluateMouse(uint64 frameId, double timestampSeconds);
    void evaluateActions(uint64 frameId, double timestampSeconds);

private:
    InputFrame m_Frame{};
    float m_FrameDeltaSeconds = 0.0f;
    std::array<bool, kInputMaxKeys> m_PreviousKeyDown{};
    std::array<bool, kInputMaxMouseButtons> m_PreviousMouseDown{};
    std::array<bool, kInputActionCount> m_PreviousActionDown{};

    TArray<InputAxisMapping, 16> m_AxisMappings;
    TArray<InputActionMapping, 16> m_ActionMappings;
    bool m_bDebugLoggingEnabled = false;
};


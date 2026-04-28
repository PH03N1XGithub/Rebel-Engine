#include "Engine/Framework/EnginePch.h"
#include "Engine/Input/PlayerInput.h"

#include <algorithm>
#include <cmath>

#include "Engine/Input/InputModule.h"

namespace
{
    constexpr float kAxisEpsilon = 0.001f;
}

DEFINE_LOG_CATEGORY(playerInputLog)

PlayerInput::PlayerInput()
{
    ResetMappings();
}

void PlayerInput::ResetMappings()
{
    m_AxisMappings.Clear();
    m_ActionMappings.Clear();

    m_AxisMappings.Emplace(InputAxisMapping{InputAction::MoveForward, GLFW_KEY_W, 1.0f});
    m_AxisMappings.Emplace(InputAxisMapping{InputAction::MoveForward, GLFW_KEY_S, -1.0f});
    m_AxisMappings.Emplace(InputAxisMapping{InputAction::MoveRight, GLFW_KEY_D, 1.0f});
    m_AxisMappings.Emplace(InputAxisMapping{InputAction::MoveRight, GLFW_KEY_A, -1.0f});
    m_AxisMappings.Emplace(InputAxisMapping{InputAction::LookYaw, InputAxisCodes::MouseX, -0.001f});
    m_AxisMappings.Emplace(InputAxisMapping{InputAction::LookPitch, InputAxisCodes::MouseY, -0.001f});

    m_ActionMappings.Emplace(InputActionMapping{InputAction::Jump, GLFW_KEY_SPACE});
    m_ActionMappings.Emplace(InputActionMapping{InputAction::Sprint, GLFW_KEY_LEFT_SHIFT});
}

void PlayerInput::AddAxisMapping(const InputAxisMapping& mapping)
{
    m_AxisMappings.Emplace(mapping);
}

void PlayerInput::AddActionMapping(const InputActionMapping& mapping)
{
    m_ActionMappings.Emplace(mapping);
}

void PlayerInput::EvaluateFrame(uint64 frameId, double timestampSeconds)
{
    const double previousTimestampSeconds = m_Frame.TimestampSeconds;
    m_Frame.FrameId = frameId;
    m_Frame.TimestampSeconds = timestampSeconds;
    m_FrameDeltaSeconds = previousTimestampSeconds > 0.0
        ? static_cast<float>(glm::max(0.0, timestampSeconds - previousTimestampSeconds))
        : 0.0f;

    evaluateKeys(frameId, timestampSeconds);
    evaluateMouse(frameId, timestampSeconds);
    evaluateActions(frameId, timestampSeconds);
}

float PlayerInput::GetAxis(const InputAction action) const
{
    return m_Frame.Actions[ToInputActionIndex(action)].Value;
}

bool PlayerInput::IsActionDown(const InputAction action) const
{
    return m_Frame.Actions[ToInputActionIndex(action)].Down;
}

bool PlayerInput::WasActionPressed(const InputAction action) const
{
    return m_Frame.Actions[ToInputActionIndex(action)].PressedThisFrame;
}

bool PlayerInput::WasActionReleased(const InputAction action) const
{
    return m_Frame.Actions[ToInputActionIndex(action)].ReleasedThisFrame;
}

const ActionState& PlayerInput::GetActionState(const InputAction action) const
{
    return m_Frame.Actions[ToInputActionIndex(action)];
}

void PlayerInput::evaluateKeys(const uint64 frameId, const double timestampSeconds)
{
    for (size_t key = 0; key < kInputMaxKeys; ++key)
    {
        KeyState& keyState = m_Frame.Keys[key];
        const bool isDown = InputModule::IsKeyPressed(static_cast<uint32>(key));
        const bool wasDown = m_PreviousKeyDown[key];

        keyState.Down = isDown;
        keyState.PressedThisFrame = isDown && !wasDown;
        keyState.ReleasedThisFrame = !isDown && wasDown;
        keyState.Value = isDown ? 1.0f : 0.0f;
        keyState.FrameId = frameId;
        keyState.TimestampSeconds = timestampSeconds;

        m_PreviousKeyDown[key] = isDown;
    }

    if (m_bDebugLoggingEnabled && GLFW_KEY_W < static_cast<int>(kInputMaxKeys))
    {
        const KeyState& wState = m_Frame.Keys[GLFW_KEY_W];
        /*RB_LOG(playerInputLog, info,
            "PlayerInput KeyW | Frame={} Down={} PressedThisFrame={} ReleasedThisFrame={}",
            frameId,
            wState.Down ? 1 : 0,
            wState.PressedThisFrame ? 1 : 0,
            wState.ReleasedThisFrame ? 1 : 0)*/
    }
}

void PlayerInput::evaluateMouse(const uint64 frameId, const double timestampSeconds)
{
    m_Frame.Mouse.X = InputModule::GetMouseX();
    m_Frame.Mouse.Y = InputModule::GetMouseY();

    const InputModule::MouseDelta delta = InputModule::GetMouseDelta();
    m_Frame.Mouse.DeltaX = delta.x;
    m_Frame.Mouse.DeltaY = delta.y;
    m_Frame.Mouse.WheelY = InputModule::GetScrollY();

    for (size_t button = 0; button < kInputMaxMouseButtons; ++button)
    {
        KeyState& buttonState = m_Frame.Mouse.Buttons[button];
        const bool isDown = InputModule::IsMouseButtonPressed(static_cast<uint32>(button));
        const bool wasDown = m_PreviousMouseDown[button];

        buttonState.Down = isDown;
        buttonState.PressedThisFrame = isDown && !wasDown;
        buttonState.ReleasedThisFrame = !isDown && wasDown;
        buttonState.Value = isDown ? 1.0f : 0.0f;
        buttonState.FrameId = frameId;
        buttonState.TimestampSeconds = timestampSeconds;

        m_PreviousMouseDown[button] = isDown;
    }
}

void PlayerInput::evaluateActions(const uint64 frameId, const double timestampSeconds)
{
    std::array<float, kInputActionCount> axisAccumulator{};
    std::array<bool, kInputActionCount> actionDownAccumulator{};
    std::array<bool, kInputActionCount> bHasMouseDeltaSource{};

    const float safeFrameDeltaSeconds = m_FrameDeltaSeconds > kAxisEpsilon ? m_FrameDeltaSeconds : 0.0f;

    for (const InputAxisMapping& mapping : m_AxisMappings)
    {
        const size_t actionIndex = ToInputActionIndex(mapping.Action);
        if (actionIndex >= kInputActionCount)
            continue;

        float rawValue = 0.0f;

        if (mapping.Key < kInputMaxKeys)
        {
            rawValue = m_Frame.Keys[mapping.Key].Value;
        }
        else if (mapping.Key == InputAxisCodes::MouseX)
        {
            bHasMouseDeltaSource[actionIndex] = true;
            rawValue = safeFrameDeltaSeconds > 0.0f ? (m_Frame.Mouse.DeltaX / safeFrameDeltaSeconds) : 0.0f;
        }
        else if (mapping.Key == InputAxisCodes::MouseY)
        {
            bHasMouseDeltaSource[actionIndex] = true;
            rawValue = safeFrameDeltaSeconds > 0.0f ? (m_Frame.Mouse.DeltaY / safeFrameDeltaSeconds) : 0.0f;
        }
        else if (mapping.Key == InputAxisCodes::MouseWheelY)
        {
            rawValue = m_Frame.Mouse.WheelY;
        }
        else
        {
            continue;
        }

        axisAccumulator[actionIndex] += rawValue * mapping.Scale;
    }

    for (const InputActionMapping& mapping : m_ActionMappings)
    {
        const size_t actionIndex = ToInputActionIndex(mapping.Action);
        if (mapping.Key >= kInputMaxKeys || actionIndex >= kInputActionCount)
            continue;

        if (m_Frame.Keys[mapping.Key].Down)
            actionDownAccumulator[actionIndex] = true;
    }

    for (size_t actionIndex = 0; actionIndex < kInputActionCount; ++actionIndex)
    {
        ActionState& actionState = m_Frame.Actions[actionIndex];

        float axisValue = axisAccumulator[actionIndex];
        if (!bHasMouseDeltaSource[actionIndex])
            axisValue = std::clamp(axisValue, -1.0f, 1.0f);

        const bool axisDown = std::fabs(axisValue) > kAxisEpsilon;
        const bool isDown = actionDownAccumulator[actionIndex] || axisDown;
        const bool wasDown = m_PreviousActionDown[actionIndex];

        actionState.Down = isDown;
        actionState.PressedThisFrame = isDown && !wasDown;
        actionState.ReleasedThisFrame = !isDown && wasDown;
        actionState.Value = axisDown ? axisValue : (actionDownAccumulator[actionIndex] ? 1.0f : 0.0f);
        actionState.FrameId = frameId;
        actionState.TimestampSeconds = timestampSeconds;

        m_PreviousActionDown[actionIndex] = isDown;
    }
}


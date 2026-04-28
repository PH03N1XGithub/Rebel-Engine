#pragma once

#include <cstddef>

#include "Core/CoreTypes.h"

enum class InputAction : uint8
{
    MoveForward = 0,
    MoveRight,
    LookYaw,
    LookPitch,
    Jump,
    Sprint,
    Count
};

constexpr size_t ToInputActionIndex(const InputAction action)
{
    return static_cast<size_t>(action);
}

constexpr size_t kInputActionCount = static_cast<size_t>(InputAction::Count);

namespace InputAxisCodes
{
    constexpr uint32 MouseX = 0x10000u;
    constexpr uint32 MouseY = 0x10001u;
    constexpr uint32 MouseWheelY = 0x10002u;
}

constexpr bool IsMouseAxisCode(const uint32 code)
{
    return code >= InputAxisCodes::MouseX && code <= InputAxisCodes::MouseWheelY;
}

struct InputAxisMapping
{
    InputAction Action = InputAction::MoveForward;
    uint32 Key = 0;
    float Scale = 1.0f;
};

struct InputActionMapping
{
    InputAction Action = InputAction::Jump;
    uint32 Key = 0;
};

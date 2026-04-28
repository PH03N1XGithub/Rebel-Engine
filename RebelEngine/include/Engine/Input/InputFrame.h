#pragma once

#include <array>

#include "Engine/Input/InputAction.h"

constexpr size_t kInputMaxKeys = 512;
constexpr size_t kInputMaxMouseButtons = 8;

struct KeyState
{
    bool Down = false;
    bool PressedThisFrame = false;
    bool ReleasedThisFrame = false;
    float Value = 0.0f;
    uint64 FrameId = 0;
    double TimestampSeconds = 0.0;
};

struct MouseState
{
    float X = 0.0f;
    float Y = 0.0f;
    float DeltaX = 0.0f;
    float DeltaY = 0.0f;
    float WheelY = 0.0f;
    std::array<KeyState, kInputMaxMouseButtons> Buttons{};
};

struct ActionState
{
    bool Down = false;
    bool PressedThisFrame = false;
    bool ReleasedThisFrame = false;
    float Value = 0.0f;
    uint64 FrameId = 0;
    double TimestampSeconds = 0.0;
};

struct InputFrame
{
    uint64 FrameId = 0;
    double TimestampSeconds = 0.0;
    std::array<KeyState, kInputMaxKeys> Keys{};
    MouseState Mouse{};
    std::array<ActionState, kInputActionCount> Actions{};
};


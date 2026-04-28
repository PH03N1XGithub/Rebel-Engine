#pragma once

#include "Engine/Framework/EngineReflectionExtensions.h"

enum class EGait
{
    Walk,
    Run,
    Sprint
};
REFLECT_ENUM(EGait)
    ENUM_OPTION(Walk)
    ENUM_OPTION(Run)
    ENUM_OPTION(Sprint)
END_ENUM(EGait)

enum class EStance
{
    Standing,
    Crouching
};
REFLECT_ENUM(EStance)
    ENUM_OPTION(Standing)
    ENUM_OPTION(Crouching)
END_ENUM(EStance)

enum class ERotationMode
{
    VelocityDirection,
    LookingDirection,
    Aiming
};
REFLECT_ENUM(ERotationMode)
    ENUM_OPTION(VelocityDirection)
    ENUM_OPTION(LookingDirection)
    ENUM_OPTION(Aiming)
END_ENUM(ERotationMode)

enum class ELocomotionState
{
    Grounded,
    InAir
};
REFLECT_ENUM(ELocomotionState)
    ENUM_OPTION(Grounded)
    ENUM_OPTION(InAir)
END_ENUM(ELocomotionState)

enum class ELocomotionAction
{
    None,
    Starting,
    Stopping,
    Pivoting,
    Jumping,
    Landing
};
REFLECT_ENUM(ELocomotionAction)
    ENUM_OPTION(None)
    ENUM_OPTION(Starting)
    ENUM_OPTION(Stopping)
    ENUM_OPTION(Pivoting)
    ENUM_OPTION(Jumping)
    ENUM_OPTION(Landing)
END_ENUM(ELocomotionAction)

struct FLocomotionState
{
    EGait Gait = EGait::Walk;
    EStance Stance = EStance::Standing;
    ERotationMode RotationMode = ERotationMode::VelocityDirection;
    ELocomotionState LocomotionState = ELocomotionState::Grounded;
    ELocomotionAction Action = ELocomotionAction::None;

    float Speed = 0.0f;
    float VerticalSpeed = 0.0f;
    float MoveDirectionLocal = 0.0f;
    float VelocityYawDelta = 0.0f;
    float AimYawDelta = 0.0f;

    bool bHasMovementInput = false;
    bool bIsMoving = false;
    bool bJustJumped = false;
    bool bJustLanded = false;
};

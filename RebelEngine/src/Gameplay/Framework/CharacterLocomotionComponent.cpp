#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/CharacterLocomotionComponent.h"

#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Gameplay/Framework/Controller.h"

namespace
{
    constexpr float kTinyNumber = 1.0e-6f;
    constexpr float kMovingEnterSpeedThreshold = 0.18f;
    constexpr float kMovingExitSpeedThreshold = 0.10f;
    constexpr float kRunSpeedThreshold = 2.5f;
    constexpr float kSprintSpeedThreshold = 4.75f;
    constexpr float kStartingMinSpeed = 0.20f;
    constexpr float kStoppingSpeedThreshold = 0.22f;
    constexpr float kStoppingMinSpeedDrop = 0.04f;
    constexpr float kPivotMinSpeed = 0.35f;
    constexpr float kPivotFacingOpposeThresholdDeg = 120.0f;
    constexpr float kPivotDirectionChangeThresholdDeg = 110.0f;
}

void CharacterLocomotionComponent::BeginPlay()
{
    ActorComponent::BeginPlay();
    ResolveOwnerReferences();

    m_State = {};
    m_PreviousState = m_State;
    m_bHasPreviousState = false;
}

void CharacterLocomotionComponent::Tick(const float deltaTime)
{
    if (deltaTime <= 0.0f)
        return;

    ResolveOwnerReferences();
    if (!m_Character || !m_CharacterMovement)
        return;

    m_PreviousState = m_State;
    UpdateLocomotionState();
    m_bHasPreviousState = true;
}

void CharacterLocomotionComponent::ResolveOwnerReferences()
{
    if (!m_Character)
        m_Character = dynamic_cast<Character*>(GetOwner());

    if (!m_CharacterMovement && m_Character)
        m_CharacterMovement = m_Character->GetCharacterMovementComponent();
}

void CharacterLocomotionComponent::UpdateLocomotionState()
{
    if (!m_Character || !m_CharacterMovement)
        return;

    const MovementState& movementState = m_CharacterMovement->GetState();
    const Vector3 velocity = movementState.Velocity;
    const Vector2 horizontalVelocity(velocity.x, velocity.y);
    const float horizontalSpeed = glm::length(horizontalVelocity);
    const bool bIsGrounded = movementState.bGrounded;

    m_State.Speed = horizontalSpeed;
    m_State.VerticalSpeed = velocity.z;
    m_State.bHasMovementInput = m_CharacterMovement->HasMovementInputThisFrame();
    m_State.bIsMoving = ComputeIsMoving(horizontalSpeed);
    m_State.LocomotionState = bIsGrounded ? ELocomotionState::Grounded : ELocomotionState::InAir;
    m_State.Stance = EStance::Standing;
    m_State.RotationMode = ERotationMode::VelocityDirection;

    if (m_State.bIsMoving)
    {
        const Vector3 planarVelocityDir = Vector3(
            horizontalVelocity.x / horizontalSpeed,
            horizontalVelocity.y / horizontalSpeed,
            0.0f);

        Vector3 forward = m_Character->GetActorForwardVector();
        Vector3 right = m_Character->GetActorRightVector();
        forward.z = 0.0f;
        right.z = 0.0f;

        const float forwardLengthSq = glm::dot(forward, forward);
        const float rightLengthSq = glm::dot(right, right);
        if (forwardLengthSq > kTinyNumber && rightLengthSq > kTinyNumber)
        {
            forward /= glm::sqrt(forwardLengthSq);
            right /= glm::sqrt(rightLengthSq);

            const float localForward = glm::dot(planarVelocityDir, forward);
            const float localRight = glm::dot(planarVelocityDir, right);
            m_State.MoveDirectionLocal = glm::degrees(std::atan2(localRight, localForward));
        }
        else
        {
            m_State.MoveDirectionLocal = 0.0f;
        }
    }
    else
    {
        m_State.MoveDirectionLocal = 0.0f;
    }

    const float actorYaw = m_Character->GetActorRotationEuler().z;
    if (m_State.bIsMoving)
    {
        const float velocityYaw = ComputePlanarYawDegrees(velocity);
        m_State.VelocityYawDelta = NormalizeDegrees(velocityYaw - actorYaw);
    }
    else
    {
        m_State.VelocityYawDelta = 0.0f;
    }

    if (const Controller* controller = m_Character->GetController())
        m_State.AimYawDelta = NormalizeDegrees(controller->GetControlRotation().z - actorYaw);
    else
        m_State.AimYawDelta = 0.0f;

    if (!m_State.bIsMoving)
    {
        m_State.Gait = EGait::Walk;
    }
    else if (horizontalSpeed < kRunSpeedThreshold)
    {
        m_State.Gait = EGait::Walk;
    }
    else if (horizontalSpeed < kSprintSpeedThreshold)
    {
        m_State.Gait = EGait::Run;
    }
    else
    {
        m_State.Gait = EGait::Sprint;
    }

    const bool bTransitionJumped = m_bHasPreviousState &&
        m_PreviousState.LocomotionState == ELocomotionState::Grounded &&
        m_State.LocomotionState == ELocomotionState::InAir;
    const bool bTransitionLanded = m_bHasPreviousState &&
        m_PreviousState.LocomotionState == ELocomotionState::InAir &&
        m_State.LocomotionState == ELocomotionState::Grounded;

    m_State.bJustJumped = bTransitionJumped || m_CharacterMovement->DidJumpThisFrame();
    m_State.bJustLanded = bTransitionLanded || m_CharacterMovement->DidLandThisFrame();
    m_State.Action = ClassifyAction();
}

float CharacterLocomotionComponent::NormalizeDegrees(float angleDegrees)
{
    float normalized = std::fmod(angleDegrees, 360.0f);
    if (normalized > 180.0f)
        normalized -= 360.0f;
    else if (normalized < -180.0f)
        normalized += 360.0f;

    return normalized;
}

float CharacterLocomotionComponent::ComputePlanarYawDegrees(const Vector3& direction)
{
    return glm::degrees(std::atan2(direction.y, direction.x));
}

bool CharacterLocomotionComponent::ComputeIsMoving(const float horizontalSpeed) const
{
    if (m_bHasPreviousState && m_PreviousState.bIsMoving)
        return horizontalSpeed > kMovingExitSpeedThreshold;

    return horizontalSpeed > kMovingEnterSpeedThreshold;
}

ELocomotionAction CharacterLocomotionComponent::ClassifyAction() const
{
    if (m_State.bJustJumped)
        return ELocomotionAction::Jumping;

    if (m_State.bJustLanded)
        return ELocomotionAction::Landing;

    if (ShouldClassifyPivoting())
        return ELocomotionAction::Pivoting;

    if (ShouldClassifyStarting())
        return ELocomotionAction::Starting;

    if (ShouldClassifyStopping())
        return ELocomotionAction::Stopping;

    return ELocomotionAction::None;
}

bool CharacterLocomotionComponent::ShouldClassifyPivoting() const
{
    if (!m_bHasPreviousState)
        return false;

    if (m_State.LocomotionState != ELocomotionState::Grounded)
        return false;

    if (!m_State.bIsMoving || !m_State.bHasMovementInput || m_State.Speed < kPivotMinSpeed)
        return false;

    const float absFacingDelta = glm::abs(m_State.VelocityYawDelta);
    const float moveDirectionDelta = glm::abs(NormalizeDegrees(m_State.MoveDirectionLocal - m_PreviousState.MoveDirectionLocal));

    const bool bStrongFacingOpposition = absFacingDelta >= kPivotFacingOpposeThresholdDeg;
    const bool bStrongDirectionChange = m_PreviousState.bIsMoving && moveDirectionDelta >= kPivotDirectionChangeThresholdDeg;
    return bStrongFacingOpposition || bStrongDirectionChange;
}

bool CharacterLocomotionComponent::ShouldClassifyStarting() const
{
    if (!m_bHasPreviousState)
        return false;

    if (m_State.LocomotionState != ELocomotionState::Grounded)
        return false;

    const bool bWasMoving = m_PreviousState.bIsMoving;
    const bool bNowMoving = m_State.bIsMoving && m_State.Speed >= kStartingMinSpeed;
    const bool bSpeedIncreasing = m_State.Speed > (m_PreviousState.Speed + kStoppingMinSpeedDrop);
    return !bWasMoving && bNowMoving && m_State.bHasMovementInput && bSpeedIncreasing;
}

bool CharacterLocomotionComponent::ShouldClassifyStopping() const
{
    if (!m_bHasPreviousState)
        return false;

    if (m_State.LocomotionState != ELocomotionState::Grounded)
        return false;

    if (!m_PreviousState.bIsMoving)
        return false;

    const bool bInputReleased = !m_State.bHasMovementInput;
    const bool bReachedLowSpeed = !m_State.bIsMoving || m_State.Speed <= kStoppingSpeedThreshold;
    const bool bSpeedDropping = m_State.Speed < (m_PreviousState.Speed - kStoppingMinSpeedDrop);
    return bInputReleased || (bReachedLowSpeed && bSpeedDropping);
}

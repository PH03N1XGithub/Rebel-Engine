#pragma once

#include "Engine/Animation/AnimInstance.h"
#include "Engine/Gameplay/Framework/MovementComponent.h"
#include "Engine/Physics/Trace.h"

struct CapsuleComponent;

enum class MovementMode
{
    Walking,
    Falling,
    Flying,
    Custom
};

enum class CharacterRotationMode
{
    None,
    OrientToMovement,
    UseControllerDesiredRotation
};

REFLECT_ENUM(MovementMode)
    ENUM_OPTION(Walking)
    ENUM_OPTION(Falling)
    ENUM_OPTION(Flying)
    ENUM_OPTION(Custom)
END_ENUM(MovementMode)

REFLECT_ENUM(CharacterRotationMode)
    ENUM_OPTION(None)
    ENUM_OPTION(OrientToMovement)
    ENUM_OPTION(UseControllerDesiredRotation)
END_ENUM(CharacterRotationMode)

struct FloorResult
{
    bool bBlockingHit = false;
    bool bWalkableFloor = false;
    bool bPerched = false;
    TraceHit Hit{};
    Vector3 FloorNormal = Vector3(0.0f, 0.0f, 1.0f);
    float FloorDistance = 0.0f;
};

struct MovementState
{
    Vector3 Velocity = Vector3(0.0f);
    Vector3 Acceleration = Vector3(0.0f);
    bool bGrounded = false;
    MovementMode Mode = MovementMode::Falling;
    float TimeInAir = 0.0f;
    FloorResult CurrentFloor{};
};

// Transform ownership contract: CharacterMovementComponent is the sole owner of UpdatedComponent motion for Character locomotion.
class CharacterMovementComponent : public MovementComponent
{
    REFLECTABLE_CLASS(CharacterMovementComponent, MovementComponent)
public:
    void BeginPlay() override;
    void Tick(float deltaTime) override;

    const MovementState& GetState() const { return m_State; }
    const FloorResult& GetCurrentFloor() const { return m_State.CurrentFloor; }
    const Vector3& GetLastMoveInput() const { return m_LastMoveInput; }
    bool HasMovementInputThisFrame() const { return m_bHasMovementInputThisFrame; }
    bool DidJumpThisFrame() const { return m_bJumpStartedThisFrame; }
    bool DidLandThisFrame() const { return m_bLandedThisFrame; }
    int GetCurrentJumpCount() const { return m_CurrentJumpCount; }
    int GetMaxJumpCount() const { return m_MaxJumpCount; }

    AnimationLocomotionState BuildAnimationLocomotionState() const;

    void SetDebugDrawCapsule(bool bEnabled) { m_bDebugDrawCapsule = bEnabled; }
    void SetDebugDrawFloor(bool bEnabled) { m_bDebugDrawFloor = bEnabled; }
    void SetDebugDrawVelocity(bool bEnabled) { m_bDebugDrawVelocity = bEnabled; }

    void SetMaxWalkSpeed(float maxWalkSpeed) { m_MaxWalkSpeed = glm::max(0.0f, maxWalkSpeed); }
    float GetMaxWalkSpeed() const { return m_MaxWalkSpeed; }
    void SetJumpVelocity(float jumpVelocity) { m_JumpVelocity = glm::max(0.0f, jumpVelocity); }
    void SetSecondJumpVelocityMultiplier(float multiplier) { m_SecondJumpVelocityMultiplier = glm::max(0.0f, multiplier); }
    void SetMaxJumpCount(int maxJumpCount) { m_MaxJumpCount = glm::max(1, maxJumpCount); }
    void SetCoyoteTime(float coyoteTime) { m_CoyoteTime = glm::max(0.0f, coyoteTime); }
    void SetJumpBufferTime(float jumpBufferTime) { m_JumpBufferTime = glm::max(0.0f, jumpBufferTime); }
    void SetFloorSweepDistance(float sweepDistance) { m_FloorSweepDistance = glm::max(0.0f, sweepDistance); }
    void SetWalkableFloorZ(float walkableFloorZ) { m_WalkableFloorZ = glm::clamp(walkableFloorZ, 0.0f, 1.0f); }
    void SetWalkableFloorZHysteresis(float hysteresis) { m_WalkableFloorZHysteresis = glm::clamp(hysteresis, 0.0f, 0.5f); }
    void SetGroundSnapDistance(float snapDistance) { m_GroundSnapDistance = glm::max(0.0f, snapDistance); }
    void SetGroundedDistanceTolerance(float tolerance) { m_GroundedDistanceTolerance = glm::max(0.0f, tolerance); }
    void SetUphillNoInputFrictionScale(float scale) { m_UphillNoInputFrictionScale = glm::max(0.0f, scale); }
    void SetUphillNoInputBrakingScale(float scale) { m_UphillNoInputBrakingScale = glm::max(0.0f, scale); }
    void SetDownhillNoInputFrictionScale(float scale) { m_DownhillNoInputFrictionScale = glm::max(0.0f, scale); }
    void SetDownhillNoInputBrakingScale(float scale) { m_DownhillNoInputBrakingScale = glm::max(0.0f, scale); }
    void SetActiveLateralBrakingScale(float scale) { m_ActiveLateralBrakingScale = glm::max(0.0f, scale); }
    void SetInputDeadZone(float deadZone) { m_InputDeadZone = glm::clamp(deadZone, 0.0f, 0.99f); }
    void SetMinActiveInputControl(float control) { m_MinActiveInputControl = glm::clamp(control, 0.0f, 1.0f); }
    void SetMinAnalogWalkSpeedControl(float control) { m_MinAnalogWalkSpeedControl = glm::clamp(control, 0.0f, 1.0f); }
    void SetMinReverseBrakingControl(float control) { m_MinReverseBrakingControl = glm::clamp(control, 0.0f, 1.0f); }
    void SetOpposingInputBrakingScale(float scale) { m_OpposingInputBrakingScale = glm::max(0.0f, scale); }
    void SetOpposingInputAccelerationScale(float scale) { m_OpposingInputAccelerationScale = glm::max(1.0f, scale); }
    void SetActiveInputOverspeedFrictionScale(float scale) { m_ActiveInputOverspeedFrictionScale = glm::max(0.0f, scale); }
    void SetBrakingSubStepTime(float subStepTime) { m_BrakingSubStepTime = glm::clamp(subStepTime, 0.001f, 0.05f); }
    void SetRotationMode(CharacterRotationMode rotationMode) { m_RotationMode = rotationMode; }
    void SetRotationRateYaw(float rotationRateYaw) { m_RotationRateYaw = glm::max(0.0f, rotationRateYaw); }
    void SetMinRotationInputThreshold(float threshold) { m_MinRotationInputThreshold = glm::max(0.0f, threshold); }
    void SetMinRotationSpeedThreshold(float threshold) { m_MinRotationSpeedThreshold = glm::max(0.0f, threshold); }
    void SetRotateInAir(bool bRotateInAir) { m_bRotateInAir = bRotateInAir; }
    void SetAirRotationRateMultiplier(float airRotationRateMultiplier) { m_AirRotationRateMultiplier = glm::max(0.0f, airRotationRateMultiplier); }
    void SetUseInputDirectionForRotation(bool bUseInputDirectionForRotation) { m_bUseInputDirectionForRotation = bUseInputDirectionForRotation; }
    void SetGravityScale(float gravityScale) { m_GravityScale = glm::max(0.0f, gravityScale); }
    void SetVelocity(const Vector3& velocity)
    {
        MovementComponent::SetVelocity(velocity);
        m_State.Velocity = velocity;
        m_State.Acceleration = Vector3(0.0f);
        m_Acceleration = Vector3(0.0f);
    }

    CharacterRotationMode GetRotationMode() const { return m_RotationMode; }
    float GetRotationRateYaw() const { return m_RotationRateYaw; }
    float GetMinRotationInputThreshold() const { return m_MinRotationInputThreshold; }
    float GetMinRotationSpeedThreshold() const { return m_MinRotationSpeedThreshold; }
    bool GetRotateInAir() const { return m_bRotateInAir; }
    float GetAirRotationRateMultiplier() const { return m_AirRotationRateMultiplier; }
    bool GetUseInputDirectionForRotation() const { return m_bUseInputDirectionForRotation; }
    float GetGravityScale() const { return m_GravityScale; }
    void LaunchCharacter(const Vector3& launchVelocity, bool bXYOverride, bool bZOverride);
    
    void SetMovementMode(MovementMode mode){ m_State.Mode = mode;};

protected:
    void PerformMovement(float deltaTime, const Vector3& moveInput, bool bJumpRequested);
    void StartNewPhysics(float deltaTime, const Vector3& moveInput);
    void PhysWalking(float deltaTime, const Vector3& moveInput);
    void PhysFalling(float deltaTime, const Vector3& moveInput);

    FloorResult FindFloor() const;
    FloorResult FindFloorFromPosition(const Vector3& position) const;
    bool CapsuleSweepDown(const Vector3& start, float distance, TraceHit& outHit, float radiusShrink = 0.0f, float halfHeightShrink = 0.0f) const;
    bool LineTraceDown(const Vector3& start, float distance, TraceHit& outHit) const;

    bool MoveWithIterativeCollision(const Vector3& delta, Vector3& inOutVelocity, TraceHit* outBlockingHit = nullptr);
    bool MoveAlongFloor(const Vector3& delta, Vector3& inOutVelocity, TraceHit* outBlockingHit = nullptr);
    bool StepUp(const Vector3& delta, const TraceHit& blockingHit, Vector3& inOutVelocity);
    bool SafeMove(const Vector3& delta, TraceHit& outHit, float& outAppliedFraction);
    bool ResolvePenetration(const TraceHit& hit, const Vector3& moveDelta);
    float ComputeSafeMoveFraction(float hitDistance, float moveDistance) const;
    Vector3 OrientCollisionNormalForSlide(const Vector3& rawNormal, const Vector3& moveDelta, const Vector3& velocity) const;

    Vector3 ConstrainToPlane(const Vector3& vector, const Vector3& planeNormal) const;
    Vector3 ClipVelocityAgainstSurface(const Vector3& velocity, const Vector3& surfaceNormal) const;
    Vector3 ComputeGroundMovementDelta(const Vector3& desiredDelta, const Vector3& floorNormal) const;
    bool TryApplyGroundSnap();
    bool IsWalkableSurfaceNormal(const Vector3& normal) const;

    void ApplyWalking(float deltaTime, const Vector3& moveInput);
    void CalcVelocity(float deltaTime, const Vector3& moveInput);
    void ApplyFalling(float deltaTime, const Vector3& moveInput);
    void UpdateCharacterRotation(float deltaTime, const Vector3& moveInput);
    void ClampHorizontalSpeedForMode(MovementMode mode);
    void UpdateMovementModeFromFloor(const FloorResult& floorResult);
    bool ShouldUpdateRotation() const;
    bool ComputeDesiredFacingDirection(const Vector3& moveInput, Vector3& outDirection) const;
    bool ComputeDesiredYaw(const Vector3& moveInput, float& outYawDegrees) const;
    void ApplyCharacterRotation(float deltaTime, float desiredYawDegrees);

    CapsuleComponent* GetUpdatedCapsule() const;
    bool HasValidUpdatedComponentOrLog() const;

    void SyncBaseState();
    void DrawMovementDebug(const FloorResult& floor) const;
    void ValidateMovementState(const MovementState& previousState, bool bJumpRequested, float deltaTime);
    bool IsWalkableFloorForGroundedState(const FloorResult& floorResult) const;
    bool CanAttemptJump() const;
    float ComputeJumpVelocityForNextJump() const;
    
    

private:
    MovementState m_State{};
    CharacterRotationMode m_RotationMode = CharacterRotationMode::OrientToMovement;

    float m_MaxWalkSpeed = 6.0f;
    float m_JumpVelocity = 4.2f;
    float m_SecondJumpVelocityMultiplier = 1.2f;
    int m_MaxJumpCount = 1;
    int m_CurrentJumpCount = 0;
    float m_CoyoteTime = 0.12f;
    float m_JumpBufferTime = 0.10f;
    float m_JumpBufferRemaining = 0.0f;
    float m_FloorSweepDistance = 0.1f;
    float m_WalkableFloorZ = 0.7f;
    float m_WalkableFloorZHysteresis = 0.02f;
    float m_GroundSnapDistance = 0.03f;
    float m_GroundedDistanceTolerance = 0.05f;
    float m_UngroundedDistanceTolerance = 0.08f;
    float m_PenetrationResolveDistance = 0.02f;
    float m_MaxDepenetrationDistance = 0.15f;
    float m_MaxStepHeight = 0.45f; 
    float m_MaxStepDownDistance = 0.45f;
    float m_SurfaceContactOffset = 0.002f;
    float m_AirControl = 0.3f;
    float m_GravityScale = 1.0f;
    float m_MaxAcceleration = 100.0f;
    float m_GroundFriction = 10.0f;
    float m_BrakingDeceleration = 100.0f;
    float m_ActiveLateralBrakingScale = 0.35f;
    float m_InputDeadZone = 0.075f;
    float m_MinActiveInputControl = 0.15f;
    float m_MinAnalogWalkSpeedControl = 0.12f;
    float m_MinReverseBrakingControl = 0.2f;
    float m_OpposingInputBrakingScale = 1.0f;
    float m_OpposingInputAccelerationScale = 1.2f;
    float m_ActiveInputOverspeedFrictionScale = 1.0f;
    float m_BrakingSubStepTime = 1.0f / 60.0f;
    float m_RotationRateYaw = 540.0f;
    float m_MinRotationInputThreshold = 0.15f;
    float m_MinRotationSpeedThreshold = 0.1f;
    float m_AirRotationRateMultiplier = 0.5f;
    float m_UphillNoInputFrictionScale = 1.15f;
    float m_UphillNoInputBrakingScale = 1.2f;
    float m_DownhillNoInputFrictionScale = 0.9f;
    float m_DownhillNoInputBrakingScale = 0.85f;
    int m_MaxSolverIterations = 5;
    int m_MaxDepenetrationIterations = 3;

    bool m_bDebugDrawCapsule = false;
    bool m_bDebugDrawFloor = true;
    bool m_bDebugDrawVelocity = true;
    bool m_bRotateInAir = false;
    bool m_bUseInputDirectionForRotation = true;
    bool m_bHasMovementInputThisFrame = false;
    bool m_bJumpStartedThisFrame = false;
    bool m_bLandedThisFrame = false;

    int m_ZeroDisplacementWithInputFrames = 0;
    bool m_bReportedInputNoMotion = true;
    bool m_bJumpArcTestActive = true;
    bool m_bJumpArcSawRise = true;
    bool m_bJumpArcSawFall = true;
    float m_JumpArcElapsed = 0.0f;
    Vector3 m_LastMoveInput = Vector3(0.0f);
    bool m_bHasPendingLaunch = false;
    Vector3 m_PendingLaunchVelocity = Vector3(0.0f);
    bool m_bPendingLaunchXYOverride = false;
    bool m_bPendingLaunchZOverride = false;
    bool m_bLaunchProtectionActive = false;
    float m_LaunchProtectionDuration = 0.12f;
    float m_LaunchProtectionTimeRemaining = 0.0f;
    float m_LaunchProtectedHorizontalSpeed = 0.0f;
};

REFLECT_CLASS(CharacterMovementComponent, MovementComponent)
    REFLECT_PROPERTY(CharacterMovementComponent, m_RotationMode,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_MaxWalkSpeed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_JumpVelocity,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_SecondJumpVelocityMultiplier,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_MaxJumpCount,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_CoyoteTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_JumpBufferTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_MaxStepHeight,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_MaxStepDownDistance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_FloorSweepDistance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_WalkableFloorZ,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_GroundSnapDistance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_AirControl,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_GravityScale,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_MaxAcceleration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_GroundFriction,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_BrakingDeceleration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_RotationRateYaw,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_bRotateInAir,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_AirRotationRateMultiplier,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_bUseInputDirectionForRotation,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_bDebugDrawCapsule,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_bDebugDrawFloor,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_bDebugDrawVelocity,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(CharacterMovementComponent, m_LaunchProtectionDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(CharacterMovementComponent)
REFLECT_ECS_COMPONENT(CharacterMovementComponent)


























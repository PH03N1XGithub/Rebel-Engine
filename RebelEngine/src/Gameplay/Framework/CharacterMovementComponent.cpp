#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"

#include "Engine/Components/CapsuleComponent.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Physics/PhysicsDebugDraw.h"
#include "Engine/Scene/World.h"

DEFINE_LOG_CATEGORY(characterMovementLog)

namespace
{
    constexpr float kTinyNumber = 1.0e-6f;
    constexpr float kSmallMoveSqr = 1.0e-8f;

    Vector3 NormalizeOrFallback(const Vector3& vector, const Vector3& fallback)
    {
        const float lengthSq = glm::dot(vector, vector);
        if (lengthSq <= kTinyNumber)
            return fallback;

        return vector / glm::sqrt(lengthSq);
    }

    float NormalizeDegrees(float angleDegrees)
    {
        float normalized = std::fmod(angleDegrees, 360.0f);
        if (normalized > 180.0f)
            normalized -= 360.0f;
        else if (normalized < -180.0f)
            normalized += 360.0f;

        return normalized;
    }

    float ComputePlanarYawDegrees(const Vector3& direction)
    {
        return glm::degrees(std::atan2(direction.y, direction.x));
    }

    AnimationMovementMode ToAnimationMovementMode(const MovementMode mode)
    {
        switch (mode)
        {
        case MovementMode::Walking: return AnimationMovementMode::Walking;
        case MovementMode::Falling: return AnimationMovementMode::Falling;
        case MovementMode::Flying: return AnimationMovementMode::Flying;
        case MovementMode::Custom: return AnimationMovementMode::Custom;
        default: return AnimationMovementMode::None;
        }
    }
}

void CharacterMovementComponent::BeginPlay()
{
    MovementComponent::BeginPlay();

    m_State = {};
    m_State.Mode = MovementMode::Falling;

    m_bJumpArcTestActive = false;
    m_bJumpArcSawRise = false;
    m_bJumpArcSawFall = false;
    m_JumpArcElapsed = 0.0f;

    // Meter-based movement profile:
    // Distance=m, Velocity=m/s, Acceleration=m/s^2.
    // Convert legacy cm-tuned values when they are clearly out of meter scale.
    if (m_MaxWalkSpeed > 50.0f)
        m_MaxWalkSpeed /= 100.0f;
    if (m_JumpVelocity > 20.0f)
        m_JumpVelocity /= 100.0f;
    if (m_Gravity > 100.0f || m_Gravity < -100.0f)
        m_Gravity /= 100.0f;
    if (m_MoveAcceleration > 100.0f)
        m_MoveAcceleration /= 100.0f;
    if (m_Friction > 100.0f)
        m_Friction /= 100.0f;
    if (m_AirDamping > 100.0f)
        m_AirDamping /= 100.0f;

    if (m_MaxWalkSpeed <= 0.0f)
        m_MaxWalkSpeed = 6.0f;
    if (m_JumpVelocity <= 0.0f)
        m_JumpVelocity = 4.2f;
    m_SecondJumpVelocityMultiplier = glm::max(0.0f, m_SecondJumpVelocityMultiplier);
    m_MaxJumpCount = glm::max(1, m_MaxJumpCount);
    m_CoyoteTime = glm::max(0.0f, m_CoyoteTime);
    m_JumpBufferTime = glm::max(0.0f, m_JumpBufferTime);
    m_JumpBufferRemaining = 0.0f;
    m_CurrentJumpCount = 0;

    if (m_Gravity > 0.0f)
        m_Gravity = -m_Gravity;
    if (glm::abs(m_Gravity) < 0.1f)
        m_Gravity = -9.8f;

    // Keep horizontal clamp coherent across walking/falling by default.
    if (m_MaxSpeed <= 0.0f || m_MaxSpeed > 50.0f)
        m_MaxSpeed = m_MaxWalkSpeed;

    // Meter-based locomotion defaults.
    if (m_MaxAcceleration <= 0.0f)
        m_MaxAcceleration = 20.0f;
    m_MoveAcceleration = m_MaxAcceleration;
    m_AirControl = glm::clamp(m_AirControl, 0.0f, 1.0f);
    if (m_GroundFriction <= 0.0f)
        m_GroundFriction = 8.0f;
    if (m_BrakingDeceleration <= 0.0f)
        m_BrakingDeceleration = 25.0f;

    m_State.CurrentFloor = FindFloor();
    UpdateMovementModeFromFloor(m_State.CurrentFloor);
    SyncBaseState();
}

void CharacterMovementComponent::Tick(const float deltaTime)
{
    if (deltaTime <= 0.0f)
        return;

    Pawn* pawn = ResolvePawnOwner();
    if (!pawn)
        return;

    if (!HasValidUpdatedComponentOrLog())
        return;

    m_bHasMovementInputThisFrame = false;
    m_bJumpStartedThisFrame = false;
    m_bLandedThisFrame = false;
    m_LastMoveInput = Vector3(0.0f);

    const Vector3 moveInput = SanitizeMoveInput(pawn->ConsumeMovementInput());
    const bool bJumpRequested = pawn->ConsumeJumpRequested();
    const MovementState previousState = m_State;
    const Vector3 startPosition = GetUpdatedComponent()->GetWorldPosition();

    m_LastMoveInput = moveInput;
    m_bHasMovementInputThisFrame = glm::dot(moveInput, moveInput) > 1.0e-4f;

    if (bJumpRequested)
        m_JumpBufferRemaining = m_JumpBufferTime;
    else if (m_JumpBufferRemaining > 0.0f)
        m_JumpBufferRemaining = glm::max(0.0f, m_JumpBufferRemaining - deltaTime);

    PerformMovement(deltaTime, moveInput, bJumpRequested);
    UpdateCharacterRotation(deltaTime, moveInput);

    m_bLandedThisFrame = !previousState.bGrounded && m_State.bGrounded;
    if (m_bLandedThisFrame)
        m_CurrentJumpCount = 0;

    const Vector3 endPosition = GetUpdatedComponent()->GetWorldPosition();
    const float inputMagnitudeSq = glm::dot(moveInput, moveInput);
    const float displacementSq = glm::dot(endPosition - startPosition, endPosition - startPosition);
    if (inputMagnitudeSq > 0.01f && displacementSq < 1.0e-6f)
    {
        ++m_ZeroDisplacementWithInputFrames;
        if (m_ZeroDisplacementWithInputFrames >= 5 && !m_bReportedInputNoMotion)
        {
            m_bReportedInputNoMotion = true;
            RB_LOG(characterMovementLog, warn,
                "CharacterMovementComponent smoke test: input detected but no displacement for {} frames. Check capsule body type / transform ownership.",
                m_ZeroDisplacementWithInputFrames);
        }
    }
    else
    {
        m_ZeroDisplacementWithInputFrames = 0;
        m_bReportedInputNoMotion = false;
    }

    const float speedMetersPerSecond = glm::length(m_State.Velocity);
    RB_LOG(characterMovementLog, trace,
        "Movement state | Velocity(m/s)=({:.2f},{:.2f},{:.2f}) Speed(m/s)={:.2f} Grounded={} MovementMode={} FloorDistance(m)={:.3f} Walkable={}",
        m_State.Velocity.x,
        m_State.Velocity.y,
        m_State.Velocity.z,
        speedMetersPerSecond,
        m_State.bGrounded ? 1 : 0,
        static_cast<int>(m_State.Mode),
        m_State.CurrentFloor.FloorDistance,
        m_State.CurrentFloor.bWalkableFloor ? 1 : 0);
    
    ValidateMovementState(previousState, bJumpRequested, deltaTime);
    //DrawMovementDebug(m_State.CurrentFloor);
    SyncBaseState();
}

AnimationLocomotionState CharacterMovementComponent::BuildAnimationLocomotionState() const
{
    AnimationLocomotionState locomotionState{};
    locomotionState.WorldVelocity = m_State.Velocity;
    locomotionState.WorldAcceleration = m_State.Acceleration;
    locomotionState.WorldMoveInput = m_LastMoveInput;
    locomotionState.HorizontalSpeed = glm::length(Vector2(m_State.Velocity.x, m_State.Velocity.y));
    locomotionState.VerticalSpeed = m_State.Velocity.z;
    locomotionState.GroundDistance = m_State.CurrentFloor.FloorDistance;
    locomotionState.TimeInAir = m_State.TimeInAir;
    locomotionState.JumpCount = m_CurrentJumpCount;
    locomotionState.bIsGrounded = m_State.bGrounded;
    locomotionState.bHasMovementInput = m_bHasMovementInputThisFrame;
    locomotionState.bJumpStarted = m_bJumpStartedThisFrame;
    locomotionState.bLanded = m_bLandedThisFrame;
    locomotionState.MovementMode = ToAnimationMovementMode(m_State.Mode);

    if (const Pawn* pawn = ResolvePawnOwner())
    {
        locomotionState.ActorYawDegrees = pawn->GetActorRotationEuler().z;

        const Vector3 forward = pawn->GetActorForwardVector();
        const Vector3 right = pawn->GetActorRightVector();
        Vector3 planarMoveDirection(m_LastMoveInput.x, m_LastMoveInput.y, 0.0f);
        if (glm::dot(planarMoveDirection, planarMoveDirection) <= kTinyNumber)
            planarMoveDirection = Vector3(m_State.Velocity.x, m_State.Velocity.y, 0.0f);

        const float planarMoveLengthSq = glm::dot(planarMoveDirection, planarMoveDirection);
        if (planarMoveLengthSq > kTinyNumber)
        {
            planarMoveDirection /= glm::sqrt(planarMoveLengthSq);
            locomotionState.LocalMoveDirection = Vector2(
                glm::dot(planarMoveDirection, right),
                glm::dot(planarMoveDirection, forward));
        }

        if (const Controller* controller = pawn->GetController())
        {
            locomotionState.bHasControllerYaw = true;
            locomotionState.ControllerYawDegrees = controller->GetControlRotation().z;
        }
    }

    return locomotionState;
}

void CharacterMovementComponent::PerformMovement(const float deltaTime, const Vector3& moveInput, const bool bJumpRequested)
{
    const Vector3 effectiveMoveInput = m_bLaunchProtectionActive ? Vector3(0.0f) : moveInput;

    if (m_bLaunchProtectionActive)
    {
        m_LaunchProtectionTimeRemaining = glm::max(0.0f, m_LaunchProtectionTimeRemaining - deltaTime);
        if (m_LaunchProtectionTimeRemaining <= 0.0f)
        {
            m_bLaunchProtectionActive = false;
            m_LaunchProtectedHorizontalSpeed = 0.0f;
        }
    }

    if (m_bHasPendingLaunch)
    {
        Vector3 nextVelocity = m_State.Velocity;
        if (m_bPendingLaunchXYOverride)
        {
            nextVelocity.x = m_PendingLaunchVelocity.x;
            nextVelocity.y = m_PendingLaunchVelocity.y;
        }
        else
        {
            nextVelocity.x += m_PendingLaunchVelocity.x;
            nextVelocity.y += m_PendingLaunchVelocity.y;
        }

        if (m_bPendingLaunchZOverride)
            nextVelocity.z = m_PendingLaunchVelocity.z;
        else
            nextVelocity.z += m_PendingLaunchVelocity.z;

        m_State.Velocity = nextVelocity;
        m_State.Acceleration = Vector3(0.0f);
        m_State.bGrounded = false;
        m_State.Mode = MovementMode::Falling;
        m_State.TimeInAir = 0.0f;

        const float launchHorizontalSpeed = glm::length(Vector2(nextVelocity.x, nextVelocity.y));
        if (launchHorizontalSpeed > kTinyNumber && m_LaunchProtectionDuration > 0.0f)
        {
            m_bLaunchProtectionActive = true;
            m_LaunchProtectionTimeRemaining = m_LaunchProtectionDuration;
            m_LaunchProtectedHorizontalSpeed = launchHorizontalSpeed;
        }

        m_bHasPendingLaunch = false;
    }

    m_State.CurrentFloor = FindFloor();
    UpdateMovementModeFromFloor(m_State.CurrentFloor);

    const auto CanConsumeBufferedJump = [&]() -> bool
    {
        if (m_JumpBufferRemaining <= 0.0f)
            return false;

        if (m_State.bGrounded)
            return CanAttemptJump();

        const bool bWithinCoyoteWindow =
            m_CurrentJumpCount == 0 &&
            m_State.Mode == MovementMode::Falling &&
            m_State.TimeInAir <= m_CoyoteTime;

        return bWithinCoyoteWindow && CanAttemptJump();
    };

    const auto ApplyJumpIfPossible = [&]() -> bool
    {
        if (!CanAttemptJump())
            return false;

        // If the player never used the grounded/coyote jump window and is now
        // doing the first true air jump, consume the grounded jump slot first.
        if (!m_State.bGrounded &&
            m_CurrentJumpCount == 0 &&
            m_State.TimeInAir > m_CoyoteTime)
        {
            m_CurrentJumpCount = 1;
        }

        m_State.Velocity.z = ComputeJumpVelocityForNextJump();
        m_State.bGrounded = false;
        m_State.Mode = MovementMode::Falling;
        m_State.TimeInAir = 0.0f;
        ++m_CurrentJumpCount;
        m_bJumpStartedThisFrame = true;
        m_JumpBufferRemaining = 0.0f;

        RB_LOG(characterMovementLog, debug,
            "Jump impulse applied | VelocityZ={:.2f} Gravity={} Grounded={} Mode={} JumpCount={}/{}",
            m_State.Velocity.z,
            GetGravityScale() * GetGravity(),
            m_State.bGrounded ? 1 : 0,
            static_cast<int>(m_State.Mode),
            m_CurrentJumpCount,
            m_MaxJumpCount);
        return true;
    };

    // Jump impulse must happen before gravity integration in this frame.
    bool bConsumedJumpThisFrame = false;
    if (bJumpRequested)
        bConsumedJumpThisFrame = ApplyJumpIfPossible();
    else if (CanConsumeBufferedJump())
        bConsumedJumpThisFrame = ApplyJumpIfPossible();

    StartNewPhysics(deltaTime, effectiveMoveInput);

    // Refresh floor state from final transform so mode transitions happen from solved motion.
    m_State.CurrentFloor = FindFloor();
    UpdateMovementModeFromFloor(m_State.CurrentFloor);

    if (m_State.Mode == MovementMode::Walking)
    {
        if (TryApplyGroundSnap())
        {
            m_State.CurrentFloor = FindFloor();
            UpdateMovementModeFromFloor(m_State.CurrentFloor);
        }
    }

    // If we were buffering a jump while falling and became jump-eligible during movement
    // resolution (e.g. landed this frame), consume the buffered request immediately.
    if (!bConsumedJumpThisFrame && CanConsumeBufferedJump())
        ApplyJumpIfPossible();
}

void CharacterMovementComponent::StartNewPhysics(const float deltaTime, const Vector3& moveInput)
{
    switch (m_State.Mode)
    {
    case MovementMode::Walking:
        PhysWalking(deltaTime, moveInput);
        break;
    case MovementMode::Flying:
        PhysWalking(deltaTime, moveInput);
        break;
    case MovementMode::Custom:
        PhysWalking(deltaTime, moveInput);
        break;
    case MovementMode::Falling:
    default:
        PhysFalling(deltaTime, moveInput);
        break;
    }
}

void CharacterMovementComponent::PhysWalking(const float deltaTime, const Vector3& moveInput)
{
    ApplyWalking(deltaTime, moveInput);
    // Walking state velocity remains horizontal. Any vertical displacement while grounded
    // must come from floor projection/step-up/snap, not from stored velocity.
    m_State.Velocity.z = 0.0f;

    const Vector3 start = GetUpdatedComponent()->GetWorldPosition();
    const Vector3 horizontalMove(m_State.Velocity.x * deltaTime, m_State.Velocity.y * deltaTime, 0.0f);

    Vector3 solvedVelocity = m_State.Velocity;
    // Walking uses a dedicated floor-aware path around the generic solver. That lets us
    // preserve the tuned velocity model while treating ramps and small steps differently
    // from falling collisions, where generic sweep-and-slide is still appropriate.
    MoveAlongFloor(horizontalMove, solvedVelocity, nullptr);

    m_State.CurrentFloor = FindFloor();
    UpdateMovementModeFromFloor(m_State.CurrentFloor);

    const Vector3 end = GetUpdatedComponent()->GetWorldPosition();
    const Vector3 actualVelocity = (end - start) / glm::max(deltaTime, kTinyNumber);

    // Walking keeps vertical velocity locked; horizontal velocity is driven by solved displacement.
    m_State.Velocity = solvedVelocity;
    /*m_State.Velocity.x = actualVelocity.x;
    m_State.Velocity.y = actualVelocity.y;*/ 
    m_State.Velocity.z = 0.0f;
    m_State.TimeInAir = 0.0f;
}

void CharacterMovementComponent::PhysFalling(const float deltaTime, const Vector3& moveInput)
{
    ApplyFalling(deltaTime, moveInput);

    const Vector3 start = GetUpdatedComponent()->GetWorldPosition();

    // Falling intentionally stays on the generic sweep-and-slide path. Walking gets a separate
    // floor-aware resolver because ground locomotion needs ramp, curb, and stair semantics that
    // would be incorrect for airborne collisions.
    const Vector3 desiredMove = m_State.Velocity * deltaTime;
    Vector3 solvedVelocity = m_State.Velocity;
    MoveWithIterativeCollision(desiredMove, solvedVelocity, nullptr);

    const Vector3 end = GetUpdatedComponent()->GetWorldPosition();
    const Vector3 actualVelocity = (end - start) / glm::max(deltaTime, kTinyNumber);

    // Falling uses solved displacement velocity to keep collision response and state coherent.
    m_State.Velocity = solvedVelocity;
    m_State.Velocity.x = actualVelocity.x;
    m_State.Velocity.y = actualVelocity.y;
    m_State.Velocity.z = actualVelocity.z;
    m_State.TimeInAir += deltaTime;
}

FloorResult CharacterMovementComponent::FindFloor() const
{
    if (!HasValidUpdatedComponent())
        return {};

    return FindFloorFromPosition(GetUpdatedComponent()->GetWorldPosition());
}

FloorResult CharacterMovementComponent::FindFloorFromPosition(const Vector3& position) const
{
    FloorResult floor{};

    const float probeDistance = glm::max(glm::max(m_FloorSweepDistance, m_MaxStepDownDistance), m_MaxStepHeight + m_SurfaceContactOffset);
    CapsuleComponent* capsule = GetUpdatedCapsule();
    if (!capsule)
        return floor;

    const float capsuleRadius = capsule->GetCapsuleRadius();
    const float capsuleHalfHeight = capsule->GetCapsuleHalfHeight();

    // Shrink the floor probe slightly so adjacent walls are less likely to be interpreted as
    // support. This is a lighter-weight version of Unreal's shrunken floor sweep.
    const float radiusShrink = glm::min(capsuleRadius * 0.15f, glm::max(m_SurfaceContactOffset * 2.0f, 0.02f));
    const float halfHeightShrink = glm::min(
        glm::max(m_SurfaceContactOffset * 2.0f, 0.02f),
        glm::max(0.0f, capsuleHalfHeight - capsuleRadius) * 0.5f);
    const float perchRadiusShrink = glm::min(
        capsuleRadius * 0.4f,
        glm::max(radiusShrink + 0.02f, capsuleRadius * 0.25f));

    auto BuildFloorResultFromHit = [&](const TraceHit& hit, const float heightShrink, const bool bPerched) -> FloorResult
    {
        FloorResult result{};
        result.bBlockingHit = hit.bBlockingHit;
        result.bWalkableFloor = hit.bBlockingHit && IsWalkableSurfaceNormal(hit.Normal);
        result.bPerched = bPerched;
        result.Hit = hit;
        result.FloorNormal = NormalizeOrFallback(hit.Normal, Vector3(0.0f, 0.0f, 1.0f));
        result.FloorDistance = glm::max(0.0f, hit.Distance - heightShrink);
        return result;
    };

    TraceHit floorHit{};
    if (CapsuleSweepDown(position, probeDistance + halfHeightShrink, floorHit, radiusShrink, halfHeightShrink))
    {
        floor = BuildFloorResultFromHit(floorHit, halfHeightShrink, false);

        if (floor.bWalkableFloor && floor.FloorDistance <= m_GroundedDistanceTolerance)
            return floor;
    }

    TraceHit lineHit{};
    if (LineTraceDown(position, probeDistance + capsuleHalfHeight, lineHit))
    {
        const Vector3 lineNormal = NormalizeOrFallback(lineHit.Normal, Vector3(0.0f, 0.0f, 1.0f));
        const float lineFloorDistance = glm::max(0.0f, lineHit.Distance - capsuleHalfHeight);
        const bool bLineWalkable = IsWalkableSurfaceNormal(lineNormal);

        if (!floor.bBlockingHit || bLineWalkable)
        {
            floor.bBlockingHit = lineHit.bBlockingHit;
            floor.bWalkableFloor = bLineWalkable;
            floor.Hit = lineHit;
            floor.FloorNormal = lineNormal;
            floor.FloorDistance = lineFloorDistance;
        }
    }

    const bool bNeedsPerchCheck = !floor.bWalkableFloor || floor.FloorDistance > m_GroundedDistanceTolerance;
    if (bNeedsPerchCheck)
    {
        TraceHit perchHit{};
        if (CapsuleSweepDown(position, probeDistance + halfHeightShrink, perchHit, perchRadiusShrink, halfHeightShrink))
        {
            FloorResult perchedFloor = BuildFloorResultFromHit(perchHit, halfHeightShrink, true);
            if (perchedFloor.bWalkableFloor && perchedFloor.FloorDistance <= m_GroundedDistanceTolerance)
                return perchedFloor;
        }
    }

    return floor;
}

bool CharacterMovementComponent::CapsuleSweepDown(const Vector3& start, const float distance, TraceHit& outHit, const float radiusShrink, const float halfHeightShrink) const
{
    outHit = {};

    CapsuleComponent* capsule = GetUpdatedCapsule();
    if (!capsule)
        return false;

    Pawn* pawn = ResolvePawnOwner();
    if (!pawn)
        return false;

    World* world = pawn->GetWorld();
    if (!world)
        return false;

    const Vector3 end = start - Vector3(0.0f, 0.0f, glm::max(0.0f, distance));

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;

    const EntityID ownerHandle = pawn->GetHandle();
    params.IgnoreEntities = &ownerHandle;
    params.IgnoreEntityCount = 1;

    const float sweepRadius = glm::max(0.001f, capsule->GetCapsuleRadius() - glm::max(0.0f, radiusShrink));
    const float sweepHalfHeight = glm::max(sweepRadius, capsule->GetCapsuleHalfHeight() - glm::max(0.0f, halfHeightShrink));
    return world->CapsuleTraceSingle(start, end, sweepHalfHeight, sweepRadius, outHit, params);
}

bool CharacterMovementComponent::LineTraceDown(const Vector3& start, const float distance, TraceHit& outHit) const
{
    outHit = {};

    Pawn* pawn = ResolvePawnOwner();
    if (!pawn)
        return false;

    World* world = pawn->GetWorld();
    if (!world)
        return false;

    const Vector3 end = start - Vector3(0.0f, 0.0f, glm::max(0.0f, distance));

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;

    const EntityID ownerHandle = pawn->GetHandle();
    params.IgnoreEntities = &ownerHandle;
    params.IgnoreEntityCount = 1;

    return world->LineTraceSingle(start, end, outHit, params);
}

bool CharacterMovementComponent::MoveWithIterativeCollision(const Vector3& delta, Vector3& inOutVelocity, TraceHit* outBlockingHit)
{
    if (outBlockingHit)
        *outBlockingHit = {};

    if (!HasValidUpdatedComponent())
        return false;

    const bool bWalkingMode = (m_State.Mode == MovementMode::Walking);
    const bool bAllowWalkingVerticalFromFloor = bWalkingMode && glm::abs(delta.z) > kTinyNumber;
    Vector3 remainingDelta = delta;
    if (bWalkingMode && !bAllowWalkingVerticalFromFloor)
    {
        remainingDelta.z = 0.0f;
        inOutVelocity.z = 0.0f;
    }
    Vector3 previousHitNormal(0.0f);
    bool bHasPreviousHitNormal = false;

    for (int iteration = 0; iteration < m_MaxSolverIterations; ++iteration)
    {
        if (glm::dot(remainingDelta, remainingDelta) <= kSmallMoveSqr)
            break;
        if (bWalkingMode && !bAllowWalkingVerticalFromFloor)
            remainingDelta.z = 0.0f;

        TraceHit hit{};
        float appliedFraction = 0.0f;
        if (!SafeMove(remainingDelta, hit, appliedFraction))
            return false;

        if (!hit.bBlockingHit)
            return true;

        if (outBlockingHit)
            *outBlockingHit = hit;

        Vector3 hitNormal = OrientCollisionNormalForSlide(hit.Normal, remainingDelta, inOutVelocity);
        const bool bWalkableHitForWalking = bWalkingMode && IsWalkableSurfaceNormal(NormalizeOrFallback(hit.Normal, Vector3(0.0f, 0.0f, 1.0f)));
        if (bWalkingMode && !bWalkableHitForWalking)
        {
            hitNormal.z = 0.0f;
            hitNormal = NormalizeOrFallback(hitNormal, Vector3(1.0f, 0.0f, 0.0f));
        }
        inOutVelocity = ClipVelocityAgainstSurface(inOutVelocity, hitNormal);
        if (bWalkingMode)
            inOutVelocity.z = 0.0f;

        // If we hit immediately, attempt depenetration before re-trying.
        if (appliedFraction <= kTinyNumber)
        {
            if (!ResolvePenetration(hit, remainingDelta))
                return true;

            remainingDelta = ConstrainToPlane(remainingDelta, hitNormal);
            if (bWalkingMode && (!bAllowWalkingVerticalFromFloor || !bWalkableHitForWalking))
                remainingDelta.z = 0.0f;
            bHasPreviousHitNormal = false;
            continue;
        }

        const float remainingFraction = glm::clamp(1.0f - appliedFraction, 0.0f, 1.0f);
        remainingDelta *= remainingFraction;
        remainingDelta = ConstrainToPlane(remainingDelta, hitNormal);
        if (bWalkingMode && (!bAllowWalkingVerticalFromFloor || !bWalkableHitForWalking))
            remainingDelta.z = 0.0f;

        // Corner handling: if we hit another non-parallel plane, constrain to crease direction.
        if (!bWalkingMode && bHasPreviousHitNormal)
        {
            const Vector3 crease = glm::cross(previousHitNormal, hitNormal);
            if (glm::dot(crease, crease) > kTinyNumber)
            {
                const Vector3 creaseDir = glm::normalize(crease);
                remainingDelta = creaseDir * glm::dot(remainingDelta, creaseDir);
                inOutVelocity = creaseDir * glm::dot(inOutVelocity, creaseDir);
            }
        }

        previousHitNormal = hitNormal;
        bHasPreviousHitNormal = true;
    }

    return true;
}

bool CharacterMovementComponent::SafeMove(const Vector3& delta, TraceHit& outHit, float& outAppliedFraction)
{
    outHit = {};
    outAppliedFraction = 0.0f;

    if (!HasValidUpdatedComponent())
        return false;

    if (glm::dot(delta, delta) <= kSmallMoveSqr)
    {
        outAppliedFraction = 1.0f;
        return true;
    }

    CapsuleComponent* capsule = GetUpdatedCapsule();
    if (!capsule)
        return false;

    const Vector3 start = GetUpdatedComponent()->GetWorldPosition();
    const Vector3 end = start + delta;

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;

    const bool bHit = capsule->SweepCapsule(start, end, outHit, params);
    if (!bHit)
    {
        GetUpdatedComponent()->SetWorldPosition(end);
        outAppliedFraction = 1.0f;
        return true;
    }

    const float moveDistance = glm::length(delta);
    if (moveDistance <= kTinyNumber)
    {
        outAppliedFraction = 0.0f;
        return true;
    }

    const float safeFraction = ComputeSafeMoveFraction(outHit.Distance, moveDistance);
    GetUpdatedComponent()->SetWorldPosition(start + delta * safeFraction);
    outAppliedFraction = safeFraction;

    return true;
}

float CharacterMovementComponent::ComputeSafeMoveFraction(const float hitDistance, const float moveDistance) const
{
    if (moveDistance <= kTinyNumber)
        return 0.0f;

    const float clampedHitDistance = glm::clamp(hitDistance, 0.0f, moveDistance);
    const float hitFraction = clampedHitDistance / moveDistance;

    // Contact offset is in world units; convert to fraction so wall standoff is stable
    // across different move lengths/frame partitions.
    const float offsetFraction = glm::clamp(m_SurfaceContactOffset / moveDistance, 0.0f, 1.0f);
    return glm::max(0.0f, hitFraction - offsetFraction);
}

bool CharacterMovementComponent::MoveAlongFloor(const Vector3& delta, Vector3& inOutVelocity, TraceHit* outBlockingHit)
{
    if (outBlockingHit)
        *outBlockingHit = {};

    if (!HasValidUpdatedComponent())
        return false;

    if (glm::dot(delta, delta) <= kSmallMoveSqr)
        return true;

    const Vector3 floorNormal = IsWalkableFloorForGroundedState(m_State.CurrentFloor)
        ? m_State.CurrentFloor.FloorNormal
        : Vector3(0.0f, 0.0f, 1.0f);
    // Grounded movement can include Z only as a consequence of projecting horizontal intent
    // onto a valid walkable floor plane.
    const Vector3 desiredMove = ComputeGroundMovementDelta(delta, floorNormal);

    TraceHit hit{};
    float appliedFraction = 0.0f;
    if (!SafeMove(desiredMove, hit, appliedFraction))
        return false;

    if (!hit.bBlockingHit)
        return true;

    if (outBlockingHit)
        *outBlockingHit = hit;

    const float remainingFraction = glm::clamp(1.0f - appliedFraction, 0.0f, 1.0f);
    Vector3 remainingDelta = desiredMove * remainingFraction;
    if (glm::dot(remainingDelta, remainingDelta) <= kSmallMoveSqr)
        return true;

    const Vector3 hitNormal = NormalizeOrFallback(hit.Normal, Vector3(0.0f, 0.0f, 1.0f));
    // Attempt StepUp first for blocking walking contacts. StepUp internally rejects true ramp hits
    // and restores the original position/velocity on failure.
    if (StepUp(remainingDelta, hit, inOutVelocity))
    {
        
        return true;
    }

    if (IsWalkableSurfaceNormal(hitNormal))
    {
        // Ramp continuation keeps floor-projected Z on walkable surfaces only.
        const Vector3 rampDelta = ComputeGroundMovementDelta(remainingDelta, hitNormal);
        return MoveWithIterativeCollision(rampDelta, inOutVelocity, outBlockingHit);
    }

    // Unwalkable collisions (walls/step faces) must stay planar while walking.
    remainingDelta.z = 0.0f;
    Vector3 slideNormal = OrientCollisionNormalForSlide(hit.Normal, remainingDelta, inOutVelocity);
    slideNormal.z = 0.0f;
    slideNormal = NormalizeOrFallback(slideNormal, Vector3(0.0f, 0.0f, 1.0f));

    const Vector3 slideDelta = ConstrainToPlane(remainingDelta, slideNormal);
    return MoveWithIterativeCollision(slideDelta, inOutVelocity, outBlockingHit);
}

bool CharacterMovementComponent::StepUp(const Vector3& delta, const TraceHit& blockingHit, Vector3& inOutVelocity)
{
    if (!HasValidUpdatedComponent())
        return false;

    CapsuleComponent* capsule = GetUpdatedCapsule();
    if (!capsule)
        return false;

    const float maxStepHeight = glm::max(0.0f, m_MaxStepHeight);
    if (maxStepHeight <= kTinyNumber)
        return false;

    const Vector3 up(0.0f, 0.0f, 1.0f);
    const Vector3 startLocation = GetUpdatedComponent()->GetWorldPosition();

    Vector3 stepDelta(delta.x, delta.y, 0.0f);
    if (glm::dot(stepDelta, stepDelta) <= kSmallMoveSqr)
        return false;

    // Reject obviously bad candidates.
    /*if (blockingHit.Normal.z > 0.2f)
        return false;*/

    RB_LOG(characterMovementLog, info, "Step up");

    TraceHit moveHit;
    float appliedFraction = 0.0f;

    // -------------------------
    // 1) Move up
    // -------------------------
    const Vector3 upDelta = up * (maxStepHeight + m_SurfaceContactOffset);

    SafeMove(upDelta, moveHit, appliedFraction);
    if (appliedFraction < 1.0f)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: up blocked");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    // -------------------------
    // 2) Move forward
    // -------------------------
    SafeMove(stepDelta, moveHit, appliedFraction);
    if (appliedFraction < 1.0f)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: forward blocked");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    // -------------------------
    // 3) Move down
    // -------------------------
    const Vector3 downDelta = -up * (maxStepHeight + m_SurfaceContactOffset * 2.0f);

    SafeMove(downDelta, moveHit, appliedFraction);

    // For downward phase, we actually WANT to hit floor before full move finishes.
    // If appliedFraction == 1.0f, we fell all the way down without finding ground.
    if (appliedFraction >= 1.0f)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: no landing floor");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    // Validate landing surface.
    if (!moveHit.bBlockingHit)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: no landing floor");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    const Vector3 candidateCenter = GetUpdatedComponent()->GetWorldPosition();
    const FloorResult landingFloor = FindFloorFromPosition(candidateCenter);
    if (!landingFloor.bBlockingHit)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: landing candidate invalid");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    if (!landingFloor.bWalkableFloor)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: landing floor not walkable");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    if (landingFloor.FloorDistance > m_MaxStepDownDistance + m_SurfaceContactOffset)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: landing candidate invalid");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    // Optional sanity check: final rise must be within max step height.
    const float rise = candidateCenter.z - startLocation.z;
    if (rise < -kTinyNumber || rise > maxStepHeight + m_SurfaceContactOffset)
    {
        RB_LOG(characterMovementLog, info, "StepUp failed: landing candidate invalid");
        GetUpdatedComponent()->SetWorldPosition(startLocation);
        return false;
    }

    // Preserve walking velocity semantics.
    if (inOutVelocity.z < 0.0f)
        inOutVelocity.z = 0.0f;

    RB_LOG(characterMovementLog, info, "StepUp succeeded");
    return true;
}

Vector3 CharacterMovementComponent::OrientCollisionNormalForSlide(const Vector3& rawNormal, const Vector3& moveDelta, const Vector3& velocity) const
{
    Vector3 normal = NormalizeOrFallback(rawNormal, Vector3(0.0f, 0.0f, 1.0f));

    // Primary orientation uses attempted displacement. On near-tangent contacts this can be
    // ambiguous, so fall back to velocity to avoid keeping stale into-wall components.
    const float normalDotMove = glm::dot(normal, moveDelta);
    if (normalDotMove > 0.0f)
        normal = -normal;
    else if (glm::abs(normalDotMove) <= kTinyNumber && glm::dot(normal, velocity) > 0.0f)
        normal = -normal;

    return normal;
}

bool CharacterMovementComponent::ResolvePenetration(const TraceHit& hit, const Vector3& moveDelta)
{
    if (!HasValidUpdatedComponent())
        return false;

    Vector3 fallbackNormal = Vector3(0.0f, 0.0f, 1.0f);
    if (glm::dot(moveDelta, moveDelta) > kTinyNumber)
        fallbackNormal = -glm::normalize(moveDelta);

    Vector3 hitNormal = NormalizeOrFallback(hit.Normal, fallbackNormal);
    // Penetration push-out must oppose attempted movement even if trace normal is flipped.
    if (glm::dot(moveDelta, moveDelta) > kTinyNumber && glm::dot(hitNormal, moveDelta) > 0.0f)
        hitNormal = -hitNormal;
    if (m_State.Mode == MovementMode::Walking)
    {
        // Walking depenetration must never add upward displacement.
        hitNormal.z = glm::min(hitNormal.z, 0.0f);
        if (glm::dot(hitNormal, hitNormal) <= kTinyNumber)
        {
            Vector3 horizontalFallback(moveDelta.x, moveDelta.y, 0.0f);
            if (glm::dot(horizontalFallback, horizontalFallback) > kTinyNumber)
                hitNormal = -glm::normalize(horizontalFallback);
            else
                hitNormal = Vector3(0.0f, 0.0f, -1.0f);
        }
        else
            hitNormal = glm::normalize(hitNormal);
    }
    const Vector3 start = GetUpdatedComponent()->GetWorldPosition();

    // Without penetration depth from trace hits, use a bounded single-step push-out.
    // Scale depenetration by attempted move size so tiny overlaps do not get a large fixed pop.
    const float minResolveDistance = glm::max(m_SurfaceContactOffset * 2.0f, 0.001f);
    const float maxResolveDistance = glm::max(minResolveDistance, m_MaxDepenetrationDistance);
    const float preferredResolveDistance = glm::clamp(
        glm::max(m_PenetrationResolveDistance, minResolveDistance),
        minResolveDistance,
        maxResolveDistance);
    const float moveSize = glm::sqrt(glm::max(glm::dot(moveDelta, moveDelta), 0.0f));
    const float heuristicResolveDistance = glm::clamp(moveSize + minResolveDistance, minResolveDistance, preferredResolveDistance);
    const float resolveDistance = glm::clamp(heuristicResolveDistance, minResolveDistance, maxResolveDistance);
    GetUpdatedComponent()->SetWorldPosition(start + hitNormal * resolveDistance);

    return true;
}

Vector3 CharacterMovementComponent::ConstrainToPlane(const Vector3& vector, const Vector3& planeNormal) const
{
    const Vector3 normal = NormalizeOrFallback(planeNormal, Vector3(0.0f, 0.0f, 1.0f));
    return vector - normal * glm::dot(vector, normal);
}

Vector3 CharacterMovementComponent::ClipVelocityAgainstSurface(const Vector3& velocity, const Vector3& surfaceNormal) const
{
    Vector3 normal = surfaceNormal;
    if (m_State.Mode == MovementMode::Walking)
    {
        normal.z = 0.0f;
        normal = NormalizeOrFallback(normal, Vector3(1.0f, 0.0f, 0.0f));
    }
    else
    {
        normal = NormalizeOrFallback(normal, Vector3(0.0f, 0.0f, 1.0f));
    }
    const float intoSurface = glm::dot(velocity, normal);
    if (intoSurface >= 0.0f)
    {
        if (m_State.Mode == MovementMode::Walking)
            return Vector3(velocity.x, velocity.y, 0.0f);
        return velocity;
    }

    // Remove inward normal component so we stop accumulating stale "push into wall" velocity.
    Vector3 clippedVelocity = velocity - normal * intoSurface;
    if (m_State.Mode == MovementMode::Walking)
        clippedVelocity.z = 0.0f;
    return clippedVelocity;
}

Vector3 CharacterMovementComponent::ComputeGroundMovementDelta(const Vector3& desiredDelta, const Vector3& floorNormal) const
{
    return ConstrainToPlane(desiredDelta, floorNormal);
}

bool CharacterMovementComponent::IsWalkableSurfaceNormal(const Vector3& normal) const
{
    const Vector3 normalizedNormal = NormalizeOrFallback(normal, Vector3(0.0f, 0.0f, 1.0f));
    return normalizedNormal.z >= m_WalkableFloorZ;
}

bool CharacterMovementComponent::IsWalkableFloorForGroundedState(const FloorResult& floorResult) const
{
    if (!floorResult.bBlockingHit)
        return false;

    // Use grounded-state normal hysteresis consistently across mode transitions, snap eligibility,
    // and slope braking decisions to avoid threshold jitter near steep walkable limits.
    const float walkableFloorZHysteresis = glm::clamp(m_WalkableFloorZHysteresis, 0.0f, 0.5f);
    const float walkableFloorZ = m_State.bGrounded
        ? glm::max(0.0f, m_WalkableFloorZ - walkableFloorZHysteresis)
        : m_WalkableFloorZ;
    return floorResult.FloorNormal.z >= walkableFloorZ;
}

bool CharacterMovementComponent::TryApplyGroundSnap()
{
    if (!HasValidUpdatedComponent())
        return false;

    if (m_State.Mode != MovementMode::Walking)
        return false;

    // Keep snap walkability aligned with grounded-mode hysteresis so tiny normal changes near
    // slope limits do not disable snap while walking is still valid.
    if (!IsWalkableFloorForGroundedState(m_State.CurrentFloor))
        return false;

    if (m_State.CurrentFloor.FloorDistance <= m_SurfaceContactOffset)
        return false;

    // Allow tiny upward residuals while grounded so snap and grounded-mode
    // hysteresis stay consistent near slope/step transitions.
    constexpr float kGroundSnapUpwardVelocityTolerance = 0.25f;
    if (m_State.Velocity.z > kGroundSnapUpwardVelocityTolerance)
        return false;

    // Keep snap intentionally limited to prevent visible pop/flapping.
    const float snapDistance = glm::min(m_State.CurrentFloor.FloorDistance - m_SurfaceContactOffset, m_GroundSnapDistance);
    if (snapDistance <= 0.0f)
        return false;

    const Vector3 position = GetUpdatedComponent()->GetWorldPosition();
    GetUpdatedComponent()->SetWorldPosition(position - Vector3(0.0f, 0.0f, snapDistance));
    return true;
}

void CharacterMovementComponent::ApplyWalking(const float deltaTime, const Vector3& moveInput)
{
    CalcVelocity(deltaTime, moveInput);
}

void CharacterMovementComponent::CalcVelocity(const float deltaTime, const Vector3& moveInput)
{
    Vector2 horizontalVelocity(m_State.Velocity.x, m_State.Velocity.y);
    const float preservedVerticalVelocity = m_State.Velocity.z;
    float activeInputControlForSpeedClamp = 0.0f;
    float activeInputMaxWalkSpeed = m_MaxWalkSpeed;

    if (m_bLaunchProtectionActive)
    {
        // Preserve launch-driven planar momentum while protection is active.
        // This prevents walking friction/braking from collapsing ground dash.
        const float speed = glm::length(horizontalVelocity);
        if (speed > kTinyNumber && m_LaunchProtectedHorizontalSpeed > speed)
            horizontalVelocity = (horizontalVelocity / speed) * m_LaunchProtectedHorizontalSpeed;

        m_State.Acceleration = Vector3(0.0f);
        m_State.Velocity.x = horizontalVelocity.x;
        m_State.Velocity.y = horizontalVelocity.y;
        m_State.Velocity.z = preservedVerticalVelocity;
        return;
    }

    Vector2 inputVector(moveInput.x, moveInput.y);
    const float inputLenSq = glm::dot(inputVector, inputVector);
    const float inputLength = glm::sqrt(glm::max(0.0f, inputLenSq));

    const float rawInputMagnitude = glm::clamp(inputLength, 0.0f, 1.0f);

    Vector2 inputDir(0.0f);
    if (inputLength > kTinyNumber)
        inputDir = inputVector / inputLength;

    const float deadZone = glm::clamp(m_InputDeadZone, 0.0f, 0.99f);
    const bool bHasInput = rawInputMagnitude > deadZone;
    float analogControlStrength = 0.0f;
    if (bHasInput)
    {
        // Ignore low-magnitude stick noise and remap remaining range to full control authority.
        // This keeps tiny analog jitter from triggering turn/brake corrections while preserving
        // predictable 0..1 response once input is intentional.
        const float controlStrength = glm::clamp((rawInputMagnitude - deadZone) / glm::max(1.0f - deadZone, kTinyNumber), 0.0f, 1.0f);
        analogControlStrength = controlStrength;
        const float minAnalogWalkSpeedControl = glm::clamp(m_MinAnalogWalkSpeedControl, 0.0f, 1.0f);
        activeInputMaxWalkSpeed = m_MaxWalkSpeed * glm::max(analogControlStrength, minAnalogWalkSpeedControl);
        // Keep intentional low-amplitude input responsive once outside dead-zone.
        const float activeControlStrength = glm::max(controlStrength, glm::clamp(m_MinActiveInputControl, 0.0f, 1.0f));
        activeInputControlForSpeedClamp = activeControlStrength;

        const float reverseBrakingControl = glm::max(controlStrength, glm::clamp(m_MinReverseBrakingControl, 0.0f, 1.0f));
        const float brakingSubStepTime = glm::clamp(m_BrakingSubStepTime, 0.001f, 0.05f);

        Vector2 desiredAcceleration(0.0f);
        float remainingTime = deltaTime;
        while (remainingTime > kTinyNumber)
        {
            const float stepTime = glm::min(remainingTime, brakingSubStepTime);

            // Actively remove lateral slip while input is held so turn arcs stay tight and responsive.
            // Integrating this in substeps improves turn behavior consistency on large frame times.
            const float speedAlongInput = glm::dot(horizontalVelocity, inputDir);
            Vector2 lateralVelocity = horizontalVelocity - inputDir * speedAlongInput;
            const float lateralSpeed = glm::length(lateralVelocity);
            if (lateralSpeed > kTinyNumber)
            {
                // Use exponential friction integration so turn damping stays consistent across frame rates.
                const float lateralFrictionFactor = 1.0f - glm::exp(-glm::max(0.0f, m_GroundFriction * activeControlStrength) * stepTime);
                const float lateralFrictionDrop = lateralSpeed * glm::clamp(lateralFrictionFactor, 0.0f, 1.0f);
                // Exposed scale keeps turn responsiveness tunable per character without hidden constants.
                const float lateralBrakingDrop = glm::max(0.0f, m_BrakingDeceleration) * glm::max(0.0f, m_ActiveLateralBrakingScale) * activeControlStrength * stepTime;
                const float lateralDrop = glm::min(lateralSpeed, lateralFrictionDrop + lateralBrakingDrop);
                const float newLateralSpeed = glm::max(0.0f, lateralSpeed - lateralDrop);
                lateralVelocity *= newLateralSpeed / lateralSpeed;
                horizontalVelocity = inputDir * speedAlongInput + lateralVelocity;
            }

            // Remove velocity that points opposite to current input before applying acceleration.
            // Scale braking by opposition strength so shallow steering keeps momentum while
            // full reversals still get strong braking response.
            float oppositionStrength = 0.0f;
            const float horizontalSpeed = glm::length(horizontalVelocity);
            if (horizontalSpeed > kTinyNumber)
            {
                const float velocityAlignment = glm::dot(horizontalVelocity / horizontalSpeed, inputDir);
                oppositionStrength = glm::clamp(-velocityAlignment, 0.0f, 1.0f);
            }

            const float speedAgainstInput = -glm::min(glm::dot(horizontalVelocity, inputDir), 0.0f);
            if (speedAgainstInput > 0.0f)
            {
                // Keep a small braking floor for intentional reverse input so direction changes stay
                // responsive immediately after crossing the analog dead-zone.
                // Keep reversal braking independently tunable from generic braking deceleration so
                // forward->backward snap feel can be adjusted without retuning no-input stops.
                const float maxBrakingThisStep = m_BrakingDeceleration
                    * glm::max(0.0f, m_OpposingInputBrakingScale)
                    * oppositionStrength
                    * reverseBrakingControl
                    * stepTime;
                const float brakingAmount = glm::min(speedAgainstInput, maxBrakingThisStep);
                horizontalVelocity += inputDir * brakingAmount;
            }

            // Increase acceleration authority when input opposes current velocity so full reversals
            // tighten turn arcs without changing baseline forward acceleration tuning.
            const float opposingInputAccelScale = 1.0f
                + (glm::max(1.0f, m_OpposingInputAccelerationScale) - 1.0f) * oppositionStrength;
            desiredAcceleration = inputDir * m_MaxAcceleration * activeControlStrength * opposingInputAccelScale;
            // When already at/above input-direction target speed, skip additional forward acceleration.
            // This avoids acceleration fighting the active overspeed correction path and keeps
            // high-speed input response less floaty without changing reversal behavior.
            const float speedAlongInputAfterDamping = glm::dot(horizontalVelocity, inputDir);
            if (speedAlongInputAfterDamping < activeInputMaxWalkSpeed)
                horizontalVelocity += desiredAcceleration * stepTime;

            remainingTime -= stepTime;
        }

        m_State.Acceleration = Vector3(desiredAcceleration.x, desiredAcceleration.y, 0.0f);
    }
    else
    {
        m_State.Acceleration = Vector3(0.0f);

        // No-input stopping uses slope-aware tuning so uphill movement settles quickly
        // while downhill travel keeps enough carry to avoid sticky descent feel.
        float noInputFrictionScale = 1.0f;
        float noInputBrakingScale = 1.0f;
        if (IsWalkableFloorForGroundedState(m_State.CurrentFloor))
        {
            const Vector3 gravityDir(0.0f, 0.0f, -1.0f);
            const Vector3 slopeDownDir = ConstrainToPlane(gravityDir, m_State.CurrentFloor.FloorNormal);
            const Vector2 slopeDownHorizontal(slopeDownDir.x, slopeDownDir.y);
            const float slopeDownHorizontalLenSq = glm::dot(slopeDownHorizontal, slopeDownHorizontal);
            if (slopeDownHorizontalLenSq > kTinyNumber)
            {
                const Vector2 slopeDownHorizontalDir = slopeDownHorizontal / glm::sqrt(slopeDownHorizontalLenSq);
                const float speed = glm::length(horizontalVelocity);
                if (speed > kTinyNumber)
                {
                    const Vector2 velocityDir = horizontalVelocity / speed;
                    const float downhillAlignment = glm::dot(velocityDir, slopeDownHorizontalDir);
                    if (downhillAlignment > 0.0f)
                    {
                        noInputFrictionScale = m_DownhillNoInputFrictionScale;
                        noInputBrakingScale = m_DownhillNoInputBrakingScale;
                    }
                    else if (downhillAlignment < 0.0f)
                    {
                        noInputFrictionScale = m_UphillNoInputFrictionScale;
                        noInputBrakingScale = m_UphillNoInputBrakingScale;
                    }
                }
            }
        }

        // Use finite-time braking when input is released so stopping does not asymptotically
        // "coast forever". Integrate braking in small substeps to keep stop feel more
        // consistent during hitchy/large-delta frames (similar to UE braking sub-stepping).
        const float speed = glm::length(horizontalVelocity);
        if (speed > kTinyNumber)
        {
            const float friction = glm::max(0.0f, m_GroundFriction * glm::max(0.0f, noInputFrictionScale));
            const float braking = glm::max(0.0f, m_BrakingDeceleration) * glm::max(0.0f, noInputBrakingScale);
            const float brakingSubStepTime = glm::clamp(m_BrakingSubStepTime, 0.001f, 0.05f);

            float remainingTime = deltaTime;
            float newSpeed = speed;
            while (remainingTime > kTinyNumber && newSpeed > kTinyNumber)
            {
                const float stepTime = glm::min(remainingTime, brakingSubStepTime);
                const float frictionFactor = 1.0f - glm::exp(-friction * stepTime);
                const float frictionDrop = newSpeed * glm::clamp(frictionFactor, 0.0f, 1.0f);
                const float brakingDrop = braking * stepTime;
                const float totalDrop = glm::min(newSpeed, frictionDrop + brakingDrop);
                newSpeed = glm::max(0.0f, newSpeed - totalDrop);
                remainingTime -= stepTime;
            }

            horizontalVelocity *= newSpeed / speed;
        }
        else
        {
            horizontalVelocity = Vector2(0.0f);
        }
    }

    // Step 7: clamp horizontal speed.
    const float speed = glm::length(horizontalVelocity);
    float maxHorizontalSpeed = m_MaxWalkSpeed;
    if (bHasInput)
    {
        // Keep sustained walk speed proportional to analog intent, but preserve a small
        // post-dead-zone speed floor so intentional low input does not feel sticky.
        maxHorizontalSpeed = activeInputMaxWalkSpeed;
    }

    if (speed > maxHorizontalSpeed && speed > kTinyNumber)
    {
        if (bHasInput && maxHorizontalSpeed > kTinyNumber)
        {
            // Do not instantly snap to active-input max speed when above cap (e.g. slope carry or
            // quick input magnitude changes). Decelerating toward the cap with braking substeps
            // preserves responsiveness while reducing abrupt frame-to-frame speed pops.
            // Include tunable friction-proportional damping so large overspeed states settle
            // faster and with better frame-rate consistency than constant braking alone.
            const float brakingSubStepTime = glm::clamp(m_BrakingSubStepTime, 0.001f, 0.05f);
            const float controlForOverspeed = glm::clamp(activeInputControlForSpeedClamp, 0.0f, 1.0f);
            const float brakingDeceleration = glm::max(0.0f, m_BrakingDeceleration) * controlForOverspeed;
            const float overspeedFriction = glm::max(0.0f, m_GroundFriction * m_ActiveInputOverspeedFrictionScale) * controlForOverspeed;

            float remainingTime = deltaTime;
            float newSpeed = speed;
            while (remainingTime > kTinyNumber && newSpeed > maxHorizontalSpeed + kTinyNumber)
            {
                const float stepTime = glm::min(remainingTime, brakingSubStepTime);
                const float overspeedAmount = glm::max(0.0f, newSpeed - maxHorizontalSpeed);
                const float frictionFactor = 1.0f - glm::exp(-overspeedFriction * stepTime);
                const float frictionDrop = overspeedAmount * glm::clamp(frictionFactor, 0.0f, 1.0f);
                const float brakingDrop = brakingDeceleration * stepTime;
                const float totalDrop = frictionDrop + brakingDrop;
                newSpeed = glm::max(maxHorizontalSpeed, newSpeed - totalDrop);
                remainingTime -= stepTime;
            }

            horizontalVelocity = (horizontalVelocity / speed) * newSpeed;
        }
        else if (maxHorizontalSpeed > kTinyNumber)
            horizontalVelocity = (horizontalVelocity / speed) * maxHorizontalSpeed;
        else
            horizontalVelocity = Vector2(0.0f);
    }

    // Step 8: write horizontal back; preserve Z.
    m_State.Velocity.x = horizontalVelocity.x;
    m_State.Velocity.y = horizontalVelocity.y;
    m_State.Velocity.z = preservedVerticalVelocity;
}
void CharacterMovementComponent::ApplyFalling(const float deltaTime, const Vector3& moveInput)
{
    Vector3 acceleration = ComputeAcceleration(moveInput);
    acceleration.x *= m_AirControl;
    acceleration.y *= m_AirControl;
    acceleration.z += m_Gravity * m_GravityScale;

    m_State.Acceleration = acceleration;
    m_State.Velocity += m_State.Acceleration * deltaTime;

    m_Velocity = m_State.Velocity;
    ApplyAirDamping(deltaTime);
    m_State.Velocity = m_Velocity;

    ClampHorizontalSpeedForMode(MovementMode::Falling);
}

void CharacterMovementComponent::UpdateCharacterRotation(const float deltaTime, const Vector3& moveInput)
{
    if (deltaTime <= 0.0f || !ShouldUpdateRotation())
        return;

    float desiredYawDegrees = 0.0f;
    if (!ComputeDesiredYaw(moveInput, desiredYawDegrees))
        return;

    ApplyCharacterRotation(deltaTime, desiredYawDegrees);
}

void CharacterMovementComponent::ClampHorizontalSpeedForMode(const MovementMode mode)
{
    const float maxSpeed = mode == MovementMode::Walking
        ? m_MaxWalkSpeed
        : m_MaxSpeed;

    float effectiveMaxSpeed = maxSpeed;
    if (m_bLaunchProtectionActive)
        effectiveMaxSpeed = glm::max(effectiveMaxSpeed, m_LaunchProtectedHorizontalSpeed);

    if (effectiveMaxSpeed <= 0.0f)
    {
        m_State.Velocity.x = 0.0f;
        m_State.Velocity.y = 0.0f;
        return;
    }

    Vector2 horizontalVelocity(m_State.Velocity.x, m_State.Velocity.y);
    const float speed = glm::length(horizontalVelocity);
    if (speed <= effectiveMaxSpeed || speed <= 0.0f)
        return;

    horizontalVelocity = (horizontalVelocity / speed) * effectiveMaxSpeed;
    m_State.Velocity.x = horizontalVelocity.x;
    m_State.Velocity.y = horizontalVelocity.y;
}

void CharacterMovementComponent::UpdateMovementModeFromFloor(const FloorResult& floorResult)
{
    if (m_bLaunchProtectionActive && m_State.Velocity.z > kTinyNumber)
    {
        // A positive vertical impulse (for example Jump) should immediately take ownership
        // of movement and stop launch protection from overriding mode decisions.
        m_bLaunchProtectionActive = false;
        m_LaunchProtectionTimeRemaining = 0.0f;
        m_LaunchProtectedHorizontalSpeed = 0.0f;
    }

    constexpr float kGroundingUpwardVelocityTolerance = 1.0f;
    const bool bMovingDownOrFlat = m_State.Velocity.z <= 0.0f;
    const float nearFloorTolerance = floorResult.bPerched
        ? glm::max(m_GroundedDistanceTolerance, m_MaxStepDownDistance)
        : m_GroundedDistanceTolerance;

    const bool bWalkableForMode = IsWalkableFloorForGroundedState(floorResult);
    const bool bNearWalkableFloor = bWalkableForMode
        && floorResult.FloorDistance <= nearFloorTolerance;
    const bool bSmallUpwardVelocityNearFloor = bNearWalkableFloor
        && m_State.Velocity.z > 0.0f
        && m_State.Velocity.z <= kGroundingUpwardVelocityTolerance;

    const bool bCanStepDownFromGrounded = m_State.bGrounded
        && bWalkableForMode
        && floorResult.FloorDistance <= m_MaxStepDownDistance
        && bMovingDownOrFlat;

    const bool bCanGround = bNearWalkableFloor
        && (bMovingDownOrFlat || bSmallUpwardVelocityNearFloor);

    const bool bMustUnground = !bWalkableForMode
        || floorResult.FloorDistance > m_UngroundedDistanceTolerance
        || m_State.Velocity.z > kGroundingUpwardVelocityTolerance;

    // Grounding hysteresis reduces walking/falling flapping around threshold distances.
    // Preserve grounded state across short walkable drop-offs so downhill/slope transitions
    // do not enter a one-frame falling mode before snap/step-down can resolve contact.
    const bool bGrounded = m_State.bGrounded
        ? (!bMustUnground || bCanStepDownFromGrounded)
        : bCanGround;

    m_State.bGrounded = bGrounded;
    if (m_State.Mode != MovementMode::Flying)
        m_State.Mode = bGrounded ? MovementMode::Walking : MovementMode::Falling;

    if (bGrounded && m_State.Mode != MovementMode::Flying)
        m_State.Velocity.z = 0.0f;
}

bool CharacterMovementComponent::ShouldUpdateRotation() const
{
    if (!HasValidUpdatedComponent())
        return false;

    if (m_RotationMode == CharacterRotationMode::None)
        return false;

    if (m_State.Mode == MovementMode::Falling && !m_bRotateInAir)
        return false;

    return true;
}

bool CharacterMovementComponent::ComputeDesiredFacingDirection(const Vector3& moveInput, Vector3& outDirection) const
{
    switch (m_RotationMode)
    {
    case CharacterRotationMode::OrientToMovement:
    {
        if (m_bUseInputDirectionForRotation)
        {
            const Vector3 planarInput(moveInput.x, moveInput.y, 0.0f);
            const float inputMagnitudeSq = glm::dot(planarInput, planarInput);
            if (inputMagnitudeSq < m_MinRotationInputThreshold * m_MinRotationInputThreshold)
                return false;

            outDirection = planarInput / glm::sqrt(inputMagnitudeSq);
            return true;
        }

        const Vector3 planarVelocity(m_State.Velocity.x, m_State.Velocity.y, 0.0f);
        const float speedSq = glm::dot(planarVelocity, planarVelocity);
        if (speedSq < m_MinRotationSpeedThreshold * m_MinRotationSpeedThreshold)
            return false;

        outDirection = planarVelocity / glm::sqrt(speedSq);
        return true;
    }
    case CharacterRotationMode::UseControllerDesiredRotation:
    {
        const Pawn* pawn = ResolvePawnOwner();
        if (!pawn)
            return false;

        const Controller* controller = pawn->GetController();
        if (!controller)
            return false;

        const float desiredYawDegrees = controller->GetControlRotation().z;
        const float desiredYawRadians = glm::radians(desiredYawDegrees);
        outDirection = Vector3(std::cos(desiredYawRadians), std::sin(desiredYawRadians), 0.0f);
        return true;
    }
    case CharacterRotationMode::None:
    default:
        return false;
    }
}

bool CharacterMovementComponent::ComputeDesiredYaw(const Vector3& moveInput, float& outYawDegrees) const
{
    Vector3 desiredDirection(0.0f);
    if (!ComputeDesiredFacingDirection(moveInput, desiredDirection))
        return false;

    outYawDegrees = ComputePlanarYawDegrees(desiredDirection);
    return true;
}

void CharacterMovementComponent::ApplyCharacterRotation(const float deltaTime, const float desiredYawDegrees)
{
    SceneComponent* updatedComponent = GetUpdatedComponent();
    if (!updatedComponent)
        return;

    Vector3 currentRotation = updatedComponent->GetRotationEuler();
    const float currentYawDegrees = currentRotation.z;
    const float deltaYawDegrees = NormalizeDegrees(desiredYawDegrees - currentYawDegrees);
    if (glm::abs(deltaYawDegrees) <= kTinyNumber)
        return;

    float rotationRateYaw = m_RotationRateYaw;
    if (m_State.Mode == MovementMode::Falling)
        rotationRateYaw *= m_AirRotationRateMultiplier;

    const float maxYawStepDegrees = glm::max(0.0f, rotationRateYaw) * deltaTime;
    const float appliedYawDeltaDegrees = glm::clamp(deltaYawDegrees, -maxYawStepDegrees, maxYawStepDegrees);
    if (glm::abs(appliedYawDeltaDegrees) <= kTinyNumber)
        return;

    currentRotation.z = NormalizeDegrees(currentYawDegrees + appliedYawDeltaDegrees);
    currentRotation.x = 0.0f;
    currentRotation.y = 0.0f;
    updatedComponent->SetRotationEuler(currentRotation);
}

CapsuleComponent* CharacterMovementComponent::GetUpdatedCapsule() const
{
    return dynamic_cast<CapsuleComponent*>(GetUpdatedComponent());
}

bool CharacterMovementComponent::HasValidUpdatedComponentOrLog() const
{
    if (HasValidUpdatedComponent())
        return true;

    RB_LOG(characterMovementLog, warn, "CharacterMovementComponent tick skipped: UpdatedComponent is missing.");
    return false;
}

void CharacterMovementComponent::SyncBaseState()
{
    m_Velocity = m_State.Velocity;
    m_Acceleration = m_State.Acceleration;
    m_bIsGrounded = m_State.bGrounded;
}

void CharacterMovementComponent::LaunchCharacter(const Vector3& launchVelocity, const bool bXYOverride, const bool bZOverride)
{
    m_PendingLaunchVelocity = launchVelocity;
    m_bPendingLaunchXYOverride = bXYOverride;
    m_bPendingLaunchZOverride = bZOverride;
    m_bHasPendingLaunch = true;
}

bool CharacterMovementComponent::CanAttemptJump() const
{
    if (m_MaxJumpCount <= 0)
        return false;

    if (m_State.bGrounded)
        return true;

    if (m_CurrentJumpCount == 0)
    {
        if (m_State.Mode != MovementMode::Falling)
            return false;

        if (m_State.TimeInAir <= m_CoyoteTime)
            return true;

        return m_MaxJumpCount > 1;
    }

    return m_CurrentJumpCount < m_MaxJumpCount;
}

float CharacterMovementComponent::ComputeJumpVelocityForNextJump() const
{
    const int nextJumpCount = m_CurrentJumpCount + 1;
    if (nextJumpCount == 2)
        return m_JumpVelocity * glm::max(0.0f, m_SecondJumpVelocityMultiplier);

    return m_JumpVelocity;
}

void CharacterMovementComponent::DrawMovementDebug(const FloorResult& floor) const
{
    if (!HasValidUpdatedComponent())
        return;

    const SceneComponent* updatedComponent = GetUpdatedComponent();
    const Vector3 position = updatedComponent->GetWorldPosition();

    if (m_bDebugDrawCapsule)
    {
        if (const CapsuleComponent* capsule = GetUpdatedCapsule())
        {
            DrawDebugCapsuleTrace(
                position,
                position,
                capsule->GetCapsuleHalfHeight(),
                capsule->GetCapsuleRadius(),
                nullptr);
        }
    }

    if (m_bDebugDrawFloor && floor.bBlockingHit)
    {
        PhysicsDebug::DrawLine(
            floor.Hit.Position,
            floor.Hit.Position + floor.FloorNormal * 25.0f,
            Vector3(1.0f, 1.0f, 0.0f));
    }

    if (m_bDebugDrawVelocity)
    {
        PhysicsDebug::DrawLine(
            position,
            position + m_State.Velocity * 0.05f,
            Vector3(0.0f, 1.0f, 1.0f));
    }
}

void CharacterMovementComponent::ValidateMovementState(const MovementState& previousState, const bool bJumpRequested, const float deltaTime)
{
    if (m_State.Mode == MovementMode::Walking && !m_State.bGrounded)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: Walking mode without grounded state.");
    }

    if (m_State.Mode == MovementMode::Falling && m_State.bGrounded)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: Falling mode while grounded.");
    }

    if (bJumpRequested && (previousState.bGrounded || previousState.Mode == MovementMode::Falling))
    {
        if (m_State.Mode != MovementMode::Falling || m_State.Velocity.z <= 0.0f)
        {
            RB_LOG(characterMovementLog, warn,
                "Movement test failed: Jump request did not produce upward falling motion.");
        }

        m_bJumpArcTestActive = true;
        m_bJumpArcSawRise = false;
        m_bJumpArcSawFall = false;
        m_JumpArcElapsed = 0.0f;
    }

    const bool stayedFalling = previousState.Mode == MovementMode::Falling && m_State.Mode == MovementMode::Falling;
    if (stayedFalling && !m_State.CurrentFloor.bWalkableFloor && m_State.TimeInAir + 1.0e-4f < previousState.TimeInAir)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: TimeInAir regressed during falling.");
    }

    if (!m_bJumpArcTestActive)
        return;

    m_JumpArcElapsed += glm::max(0.0f, deltaTime);

    if (m_State.Mode == MovementMode::Falling && m_State.Velocity.z > 1.0f)
        m_bJumpArcSawRise = true;

    if (m_State.Mode == MovementMode::Falling && m_State.Velocity.z < -1.0f)
        m_bJumpArcSawFall = true;

    if (m_JumpArcElapsed > 0.25f && !m_bJumpArcSawRise)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: Jump arc did not enter a rising phase.");
        m_bJumpArcTestActive = false;
        return;
    }

    if (m_JumpArcElapsed > 0.75f && m_bJumpArcSawRise && !m_bJumpArcSawFall)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: Jump arc stayed rising too long; check gravity/jump tuning.");
        m_bJumpArcTestActive = false;
        return;
    }

    if (m_State.bGrounded)
    {
        if (!m_bJumpArcSawRise || !m_bJumpArcSawFall)
        {
            RB_LOG(characterMovementLog, warn,
                "Movement test failed: Jump arc did not complete rise and fall before landing.");
        }

        m_bJumpArcTestActive = false;
        return;
    }

    if (m_JumpArcElapsed > 3.0f)
    {
        RB_LOG(characterMovementLog, warn,
            "Movement test failed: Jump arc timeout (>3s) before landing.");
        m_bJumpArcTestActive = false;
    }
}














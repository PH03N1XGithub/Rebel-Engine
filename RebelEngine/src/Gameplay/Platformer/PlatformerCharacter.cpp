#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Platformer/PlatformerCharacter.h"

#include "Engine/Components/SkeletalMeshComponent.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Physics/Trace.h"
#include "Engine/Scene/World.h"

namespace
{
    constexpr float kTinyNumber = 1.0e-6f;

    Vector3 NormalizeOrFallback(const Vector3& vector, const Vector3& fallback)
    {
        const float lengthSq = glm::dot(vector, vector);
        if (lengthSq <= kTinyNumber)
            return fallback;

        return vector / glm::sqrt(lengthSq);
    }

    bool IsRunnableWallNormal(const Vector3& normal)
    {
        const Vector3 normalized = NormalizeOrFallback(normal, Vector3(0.0f, 0.0f, 1.0f));
        return glm::abs(normalized.z) < 0.2f;
    }
}

PlatformerCharacter::~PlatformerCharacter()
{
}

void PlatformerCharacter::BeginPlay()
{
    Character::BeginPlay();

    m_bCanDash = true;
    m_DashCooldownTimerHandle.Invalidate();
    if (CharacterMovementComponent* movement = GetCharacterMovementComponent())
    {
        m_PreDashGravityScale = movement->GetGravityScale();
        m_PreWallRunGravityScale = movement->GetGravityScale();
    }
}

void PlatformerCharacter::Tick(float dt)
{
    Character::Tick(dt);

    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    World* world = GetWorld();
    if (!movement || !world)
        return;

    if (m_bWallRunning)
    {
        UpdateWallRun(dt);
        UpdateWallRunAnimation();
    }

    // If dash recovery timer ended while airborne, restore dash once grounded.
    if (!m_bCanDash &&
        !world->IsTimerActive(m_DashCooldownTimerHandle) &&
        movement->GetState().bGrounded)
    {
        m_bCanDash = true;
    }
}

void PlatformerCharacter::HandleJumpPressed()
{
    if (m_bWallRunning)
    {
        WallJump();
        return;
    }

    if (TryStartWallRun())
        return;

    Jump();
}

bool PlatformerCharacter::TryDash()
{
    if (!m_bCanDash)
        return false;

    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    if (!movement)
        return false;

    const float dashSpeed = glm::max(0.0f, m_DashSpeed);
    if (dashSpeed <= 0.0f)
        return false;

    m_bCanDash = false;

    m_PreDashGravityScale = movement->GetGravityScale();
    movement->SetGravityScale(glm::max(0.0f, m_GravityScaleWhileDashing));
    movement->SetVelocity(Vector3(0.0f));

    Vector3 dashDirection = GetPendingMovementInput();
    dashDirection.z = 0.0f;
    const float directionLengthSq = glm::dot(dashDirection, dashDirection);
    if (directionLengthSq <= 1.0e-6f)
        dashDirection = GetActorForwardVector();
    else
        dashDirection /= glm::sqrt(directionLengthSq);

    LaunchCharacter(dashDirection * dashSpeed, true, true);

    if (SkeletalMeshComponent* mesh = GetMeshComponent())
    {
        AssetHandle dashAnimHandle = m_DashAnimation.GetHandle();
        
        if ((uint64)dashAnimHandle != 0)
            mesh->PlayAnimation(
                dashAnimHandle,
                false,
                glm::max(0.0f, m_DashAnimationPlaybackSpeed),
                glm::max(0.0f, m_DashAnimationBlendIn));
    }

    if (World* world = GetWorld())
    {
        const float timerDelay = glm::max(0.0f, m_DashDuration);
        world->SetTimer(
            m_DashCooldownTimerHandle,
            [this]()
            {
                CharacterMovementComponent* dashMovement = GetCharacterMovementComponent();
                if (!dashMovement)
                {
                    m_bCanDash = true;
                    return;
                }

                const float restoredGravityScale = m_GravityScaleAfterDash > 0.0f
                    ? glm::max(0.0f, m_GravityScaleAfterDash)
                    : glm::max(0.0f, m_PreDashGravityScale);
                dashMovement->SetGravityScale(restoredGravityScale);
                m_bCanDash = dashMovement->GetState().bGrounded;
            },
            timerDelay,
            false);
    }

    return true;
}

bool PlatformerCharacter::TryStartWallRun()
{
    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    if (!movement || m_bWallRunning)
        return false;

    const MovementState& movementState = movement->GetState();
    if (movementState.bGrounded || movementState.Mode != MovementMode::Falling)
        return false;

    /*const Vector3 horizontalVelocity(movementState.Velocity.x, movementState.Velocity.y, 0.0f);
    if (glm::dot(horizontalVelocity, horizontalVelocity) <
        m_WallRunMinHorizontalSpeed * m_WallRunMinHorizontalSpeed)
    {
        return false;
    }*/

    TraceHit wallHit{};
    bool bWallOnRight = false;
    if (!TraceWall(wallHit, bWallOnRight))
        return false;

    StartWallRun(wallHit, bWallOnRight);
    return true;
}

bool PlatformerCharacter::TraceWall(TraceHit& outHit, bool& bOutWallOnRight) const
{
    const World* world = GetWorld();
    if (!world)
        return false;

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;
    const EntityID ignoredEntity = GetHandle();
    params.IgnoreEntities = &ignoredEntity;
    params.IgnoreEntityCount = 1;

    const Vector3 start = GetActorLocation();
    const Vector3 rightEnd = start + GetActorRightVector() * glm::max(0.0f, m_WallRunSideProbeDistance);
    const Vector3 leftEnd = start - GetActorRightVector() * glm::max(0.0f, m_WallRunSideProbeDistance);

    TraceHit rightHit{};
    TraceHit leftHit{};
    
    const bool bHitRight = LineTraceSingle(start, rightEnd, rightHit, params,EDrawDebugTrace::ForDuration) &&
        rightHit.bBlockingHit &&
        IsRunnableWallNormal(rightHit.Normal);
    const bool bHitLeft = LineTraceSingle(start, leftEnd, leftHit, params,EDrawDebugTrace::ForOneFrame) &&
        leftHit.bBlockingHit &&
        IsRunnableWallNormal(leftHit.Normal);

    if (!bHitRight && !bHitLeft)
        return false;

    if (bHitRight && (!bHitLeft || rightHit.Distance <= leftHit.Distance))
    {
        outHit = rightHit;
        bOutWallOnRight = true;
        return true;
    }

    outHit = leftHit;
    bOutWallOnRight = false;
    return true;
}

void PlatformerCharacter::StartWallRun(const TraceHit& wallHit, const bool bWallOnRight)
{
    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    if (!movement)
        return;

    m_bWallRunning = true;
    m_bWallOnRight = bWallOnRight;
    m_WallRunNormal = NormalizeOrFallback(wallHit.Normal, bWallOnRight ? -GetActorRightVector() : GetActorRightVector());
    m_PreWallRunGravityScale = movement->GetGravityScale();

    movement->SetGravityScale(0.0f);
    movement->SetRotationMode(CharacterRotationMode::None);
    movement->SetMovementMode(MovementMode::Flying);
    movement->SetVelocity(Vector3(0.0f));

    SetActorLocation(wallHit.Position + m_WallRunNormal * glm::max(0.0f, m_WallRunWallOffset));

    const Vector3 desiredForward = NormalizeOrFallback(
        glm::cross(Vector3(0.0f, 0.0f, 1.0f), m_WallRunNormal),
        GetActorForwardVector());
    const Vector3 actorForward = GetActorForwardVector();
    const Vector3 alignedForward = glm::dot(desiredForward, actorForward) < 0.0f ? -desiredForward : desiredForward;
    const float yawDegrees = glm::degrees(std::atan2(alignedForward.y, alignedForward.x));
    SetActorRotation(Vector3(0.0f, 0.0f, yawDegrees));

    PlayWallRunStartAnimation();
}

void PlatformerCharacter::UpdateWallRun(float dt)
{
    (void)dt;

    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    World* world = GetWorld();
    if (!movement || !world)
        return;

    if (movement->GetState().bGrounded)
    {
        StopWallRun(false);
        return;
    }

    const float forwardProbeDistance = glm::max(0.0f, m_WallRunForwardProbeDistance);
    const float sideProbeDistance = glm::max(0.0f, m_WallRunSideProbeDistance);
    const float downProbeDistance = glm::max(0.0f, m_WallRunDownProbeDistance);

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;
    const EntityID ignoredEntity = GetHandle();
    params.IgnoreEntities = &ignoredEntity;
    params.IgnoreEntityCount = 1;

    const Vector3 forward = GetActorForwardVector();
    const Vector3 side = m_bWallOnRight ? GetActorRightVector() : -GetActorRightVector();
    const Vector3 wallCheckStart = GetActorLocation() + forward * forwardProbeDistance;
    const Vector3 wallCheckEnd = wallCheckStart + side * sideProbeDistance;

    TraceHit wallHit{};
    const bool bStillOnWall = world->LineTraceSingle(wallCheckStart, wallCheckEnd, wallHit, params) &&
        wallHit.bBlockingHit &&
        IsRunnableWallNormal(wallHit.Normal);

    if (!bStillOnWall)
    {
        const Vector3 downStart = GetActorLocation() + forward * (forwardProbeDistance + 0.5f);
        const Vector3 downEnd = downStart - GetActorUpVector() * downProbeDistance;
        TraceHit downHit{};
        const bool bGroundAhead = world->LineTraceSingle(downStart, downEnd, downHit, params) &&
            downHit.bBlockingHit &&
            downHit.Normal.z > 0.5f;

        if (bGroundAhead)
        {
            StopWallRun(false);
            return;
        }

        WallJump();
        return;
    }

    m_WallRunNormal = NormalizeOrFallback(wallHit.Normal, m_WallRunNormal);

    const Vector3 wallTangent = NormalizeOrFallback(
        glm::cross(Vector3(0.0f, 0.0f, 1.0f), m_WallRunNormal),
        forward);
    const Vector3 alignedForward = glm::dot(wallTangent, forward) < 0.0f ? -wallTangent : wallTangent;
    const float yawDegrees = glm::degrees(std::atan2(alignedForward.y, alignedForward.x));
    SetActorRotation(Vector3(0.0f, 0.0f, yawDegrees));

    movement->SetGravityScale(0.0f);
    movement->SetMovementMode(MovementMode::Flying);
    movement->SetVelocity(GetActorForwardVector() * glm::max(0.0f, m_WallRunForwardSpeed));
}

void PlatformerCharacter::StopWallRun(const bool bApplyExitLaunch, const bool bPlayEndAnimation)
{
    CharacterMovementComponent* movement = GetCharacterMovementComponent();
    if (!movement)
    {
        m_bWallRunning = false;
        m_WallRunAnimationPhase = WallRunAnimationPhase::None;
        return;
    }

    m_bWallRunning = false;
    m_WallRunAnimationPhase = WallRunAnimationPhase::None;
    movement->SetGravityScale(glm::max(0.0f, m_PreWallRunGravityScale));
    movement->SetRotationMode(CharacterRotationMode::OrientToMovement);
    movement->SetMovementMode(MovementMode::Falling);

    if (bPlayEndAnimation)
        PlayWallRunEndAnimation();
    else if (SkeletalMeshComponent* mesh = GetMeshComponent())
        mesh->StopAnimation();

    if (bApplyExitLaunch)
    {
        movement->LaunchCharacter(
            GetActorForwardVector() * glm::max(0.0f, m_WallRunExitSpeed),
            true,
            false);
    }
}

void PlatformerCharacter::WallJump()
{
    if (!m_bWallRunning)
        return;

    StopWallRun(false, false);
    m_bCanDash = true;
    PlayWallJumpAnimation();

    const Vector3 sideImpulse = (m_bWallOnRight ? -GetActorRightVector() : GetActorRightVector()) *
        glm::max(0.0f, m_WallJumpSpeed);
    const Vector3 upwardImpulse = GetActorUpVector() * glm::max(0.0f, m_WallJumpUpwardSpeed);
    const Vector3 forwardImpulse = GetActorForwardVector() * glm::max(0.0f, m_WallJumpSpeed);
    LaunchCharacter(sideImpulse + upwardImpulse + forwardImpulse, true, true);
}

void PlatformerCharacter::UpdateWallRunAnimation()
{
    if (!m_bWallRunning || m_WallRunAnimationPhase != WallRunAnimationPhase::Starting)
        return;

    SkeletalMeshComponent* mesh = GetMeshComponent();
    if (!mesh || mesh->bOverrideAnimationActive)
        return;

    PlayWallRunLoopAnimation();
}

void PlatformerCharacter::PlayWallRunStartAnimation()
{
    SkeletalMeshComponent* mesh = GetMeshComponent();
    if (!mesh)
        return;

    const AssetPtr<AnimationAsset>& animation = m_bWallOnRight
        ? m_WallRunStartAnimationRight
        : m_WallRunStartAnimationLeft;

    if ((uint64)animation.GetHandle() != 0)
    {
        mesh->bOverrideLockRootBoneTranslation = true;
        mesh->PlayAnimation(
            animation,
            false,
            glm::max(0.0f, m_WallRunAnimationPlaybackSpeed),
            glm::max(0.0f, m_WallRunAnimationBlendIn));
        m_WallRunAnimationPhase = WallRunAnimationPhase::Starting;
        return;
    }

    PlayWallRunLoopAnimation();
}

void PlatformerCharacter::PlayWallRunLoopAnimation()
{
    SkeletalMeshComponent* mesh = GetMeshComponent();
    if (!mesh)
        return;

    const AssetPtr<AnimationAsset>& animation = m_bWallOnRight
        ? m_WallRunLoopAnimationRight
        : m_WallRunLoopAnimationLeft;

    if ((uint64)animation.GetHandle() == 0)
    {
        m_WallRunAnimationPhase = WallRunAnimationPhase::Looping;
        return;
    }

    mesh->bOverrideLockRootBoneTranslation = true;
    mesh->PlayAnimation(
        animation,
        true,
        glm::max(0.0f, m_WallRunAnimationPlaybackSpeed),
        glm::max(0.0f, m_WallRunAnimationBlendIn));
    m_WallRunAnimationPhase = WallRunAnimationPhase::Looping;
}

void PlatformerCharacter::PlayWallRunEndAnimation()
{
    SkeletalMeshComponent* mesh = GetMeshComponent();
    if (!mesh)
        return;

    const AssetPtr<AnimationAsset>& animation = m_bWallOnRight
        ? m_WallRunEndAnimationRight
        : m_WallRunEndAnimationLeft;

    if ((uint64)animation.GetHandle() != 0)
    {
        mesh->bOverrideLockRootBoneTranslation = true;
        mesh->PlayAnimation(
            animation,
            false,
            glm::max(0.0f, m_WallRunAnimationPlaybackSpeed),
            glm::max(0.0f, m_WallRunAnimationBlendIn));
        return;
    }

    mesh->StopAnimation();
}

void PlatformerCharacter::PlayWallJumpAnimation()
{
    SkeletalMeshComponent* mesh = GetMeshComponent();
    if (!mesh)
        return;

    const AssetPtr<AnimationAsset>& animation = m_bWallOnRight
        ? m_WallJumpAnimationRight
        : m_WallJumpAnimationLeft;

    if ((uint64)animation.GetHandle() == 0)
        return;

    mesh->bOverrideLockRootBoneTranslation = true;
    mesh->PlayAnimation(
        animation,
        false,
        glm::max(0.0f, m_WallRunAnimationPlaybackSpeed),
        glm::max(0.0f, m_WallRunAnimationBlendIn));
}

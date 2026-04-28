#include "Engine/Framework/EnginePch.h"
#include "Engine/Animation/AnimInstance.h"

#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Components/SkeletalMeshComponent.h"

namespace
{
const char* ToDebugString(const LocomotionAnimState state)
{
    switch (state)
    {
    case LocomotionAnimState::Idle: return "Idle";
    case LocomotionAnimState::Move: return "Move";
    case LocomotionAnimState::Start: return "Start";
    case LocomotionAnimState::Stop: return "Stop";
    case LocomotionAnimState::Pivot: return "Pivot";
    case LocomotionAnimState::InAir: return "InAir";
    case LocomotionAnimState::Land: return "Land";
    default: return "Unknown";
    }
}

float ComputeTransitionBlendAlpha(const float elapsedTime, const float duration)
{
    if (duration <= 1e-6f)
        return 1.0f;

    return FMath::clamp(elapsedTime / duration, 0.0f, 1.0f);
}

void BlendLocalPoses(
    const TArray<Mat4>& fromPose,
    const TArray<Mat4>& toPose,
    const float alpha,
    TArray<Mat4>& outPose)
{
    if (fromPose.Num() != toPose.Num())
    {
        outPose = toPose;
        return;
    }

    outPose.Resize(toPose.Num());
    const float blendAlpha = FMath::clamp(alpha, 0.0f, 1.0f);
    for (int32 i = 0; i < toPose.Num(); ++i)
    {
        Vector3 fromTranslation(0.0f);
        Vector3 fromScale(1.0f);
        Quaternion fromRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(fromPose[i], fromTranslation, fromRotation, fromScale);

        Vector3 toTranslation(0.0f);
        Vector3 toScale(1.0f);
        Quaternion toRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(toPose[i], toTranslation, toRotation, toScale);

        Quaternion adjustedTargetRotation = toRotation;
        if (FMath::dot(fromRotation, adjustedTargetRotation) < 0.0f)
            adjustedTargetRotation = -adjustedTargetRotation;

        const Vector3 blendedTranslation = FMath::mix(fromTranslation, toTranslation, blendAlpha);
        const Vector3 blendedScale = FMath::mix(fromScale, toScale, blendAlpha);
        const Quaternion blendedRotation =
            FMath::normalize(FMath::slerp(fromRotation, adjustedTargetRotation, blendAlpha));

        outPose[i] = AnimationRuntime::ComposeTRS(blendedTranslation, blendedRotation, blendedScale);
    }
}
}

void AnimInstance::Initialize(SkeletalMeshComponent* owningComponent)
{
    m_OwningComponent = owningComponent;
    m_LocomotionState = {};
    OnInitialize();
}

const AnimationAsset* AnimInstance::ResolveAnimationAsset(
    const AnimationEvaluationContext& context,
    const AssetPtr<AnimationAsset>& asset) const
{
    if (!context.AssetManager || (uint64)asset.GetHandle() == 0)
        return nullptr;

    AnimationAsset* animation =
        dynamic_cast<AnimationAsset*>(context.AssetManager->Load(asset.GetHandle()));
    if (!animation)
        return nullptr;

    if (context.Skeleton && (uint64)animation->m_SkeletonID != 0 &&
        (uint64)animation->m_SkeletonID != (uint64)context.Skeleton->ID)
    {
        return nullptr;
    }

    return animation;
}

bool AnimInstance::EvaluateClip(
    const AnimationEvaluationContext& context,
    const AssetPtr<AnimationAsset>& asset,
    const float playbackTime,
    const bool bLooping,
    TArray<Mat4>& outLocalPose) const
{
    const AnimationAsset* animation = ResolveAnimationAsset(context, asset);
    if (!context.Skeleton || !context.LocalBindPose || !animation)
        return false;

    const float sampleTime = AnimationRuntime::NormalizePlaybackTime(
        playbackTime,
        animation->m_DurationSeconds,
        bLooping);

    return AnimationRuntime::EvaluateLocalPose(
        context.Skeleton,
        animation,
        sampleTime,
        *context.LocalBindPose,
        outLocalPose,
        m_bRootMotionEnabled);
}

void LocomotionAnimInstance::TransitionTo(const LocomotionAnimState newState)
{
    if (m_State == newState)
        return;

    m_PreviousAnimation = SelectAnimationForState(m_State);
    m_PreviousState = m_State;
    m_PreviousStatePlaybackTime = m_StatePlaybackTime;
    m_TransitionElapsedTime = 0.0f;
    m_bTransitionActive = m_TransitionDuration > 1e-6f;

    m_State = newState;
    m_StateElapsedTime = 0.0f;
    m_StatePlaybackTime = 0.0f;
}

bool LocomotionAnimInstance::IsStateLooping(const LocomotionAnimState state)
{
    return state == LocomotionAnimState::Idle ||
           state == LocomotionAnimState::Move ||
           state == LocomotionAnimState::InAir;
}
DEFINE_LOG_CATEGORY(AnimInstanceLog)
void LocomotionAnimInstance::Update(const float dt, const AnimationLocomotionState& locomotionState)
{
    m_LocomotionState = locomotionState;
    Speed = locomotionState.HorizontalSpeed;
    VerticalSpeed = locomotionState.VerticalSpeed;
    GroundDistance = locomotionState.GroundDistance;
    TimeInAir = locomotionState.TimeInAir;
    ActorYawDegrees = locomotionState.ActorYawDegrees;
    ControllerYawDegrees = locomotionState.ControllerYawDegrees;
    MoveDirectionLocal = locomotionState.MoveDirectionLocalDegrees;
    VelocityYawDelta = locomotionState.VelocityYawDelta;
    AimYawDelta = locomotionState.AimYawDelta;
    JumpCount = locomotionState.JumpCount;
    IsGrounded = locomotionState.SemanticLocomotionState == ELocomotionState::Grounded;
    LocomotionState = locomotionState.SemanticLocomotionState;
    bIsInAir = LocomotionState == ELocomotionState::InAir;
    HasMovementInput = locomotionState.bHasMovementInput;
    bIsMoving = locomotionState.bIsMoving;
    bIsStarting = locomotionState.Action == ELocomotionAction::Starting;
    bIsStopping = locomotionState.Action == ELocomotionAction::Stopping;
    bIsPivoting = locomotionState.Action == ELocomotionAction::Pivoting;
    JumpStarted = locomotionState.bJumpStarted;
    Landed = locomotionState.bLanded;
    HasControllerYaw = locomotionState.bHasControllerYaw;
    Gait = locomotionState.Gait;
    Stance = locomotionState.Stance;
    RotationMode = locomotionState.RotationMode;
    Action = locomotionState.Action;

    const float clampedDeltaTime = glm::max(0.0f, dt);
    m_StateElapsedTime += clampedDeltaTime;
    m_StatePlaybackTime += clampedDeltaTime;
    if (m_bTransitionActive)
    {
        m_PreviousStatePlaybackTime += clampedDeltaTime;
        m_TransitionElapsedTime += clampedDeltaTime;
        if (m_TransitionElapsedTime >= m_TransitionDuration)
            m_bTransitionActive = false;
    }
    
    //RB_LOG(AnimInstanceLog,debug,"State {}",(uint8)m_State);

    if (IsInAir())
    {
        TransitionTo(LocomotionAnimState::InAir);
        return;
    }

    if (m_LocomotionState.bLanded)
    {
        TransitionTo(LocomotionAnimState::Land);
        return;
    }

    if (m_State == LocomotionAnimState::Land)
    {
        if (m_StateElapsedTime < m_LandMinDuration)
            return;
    }

    if (m_State == LocomotionAnimState::Start && m_StateElapsedTime < m_StartMinDuration)
        return;

    if (m_State == LocomotionAnimState::Stop && m_StateElapsedTime < m_StopMinDuration)
        return;

    if (m_State == LocomotionAnimState::Pivot && m_StateElapsedTime < m_PivotMinDuration)
        return;

    if (Action == ELocomotionAction::Pivoting)
    {
        TransitionTo(LocomotionAnimState::Pivot);
        return;
    }

    if (Action == ELocomotionAction::Starting)
    {
        TransitionTo(LocomotionAnimState::Start);
        return;
    }

    if (Action == ELocomotionAction::Stopping)
    {
        TransitionTo(LocomotionAnimState::Stop);
        return;
    }

    if (IsMoving())
    {
        TransitionTo(LocomotionAnimState::Move);
        return;
    }

    TransitionTo(LocomotionAnimState::Idle);
}

AssetPtr<AnimationAsset> LocomotionAnimInstance::SelectAnimationForState(const LocomotionAnimState state) const
{
    const SkeletalMeshComponent* component = GetOwningComponent();
    if (!component)
        return {};

    switch (state)
    {
    case LocomotionAnimState::Idle:
        if ((uint64)component->IdleAnimation.GetHandle() != 0)
            return component->IdleAnimation;
        break;
    case LocomotionAnimState::Move:
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::Start:
        if ((uint64)component->StartAnimation.GetHandle() != 0)
            return component->StartAnimation;
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::Stop:
        if ((uint64)component->StopLocomotionAnimation.GetHandle() != 0)
            return component->StopLocomotionAnimation;
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::Pivot:
        if ((uint64)component->PivotAnimation.GetHandle() != 0)
            return component->PivotAnimation;
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::InAir:
        if ((uint64)component->InAirAnimation.GetHandle() != 0)
            return component->InAirAnimation;
        if ((uint64)component->FallingAnimation.GetHandle() != 0)
            return component->FallingAnimation;
        if (m_LocomotionState.bJumpStarted &&
            m_LocomotionState.JumpCount >= 2 &&
            (uint64)component->DoubleJumpAnimation.GetHandle() != 0)
        {
            return component->DoubleJumpAnimation;
        }
        if (m_LocomotionState.bJumpStarted &&
            (uint64)component->JumpAnimation.GetHandle() != 0)
        {
            return component->JumpAnimation;
        }
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::Land:
        if ((uint64)component->LandAnimation.GetHandle() != 0)
            return component->LandAnimation;
        if ((uint64)component->StopLocomotionAnimation.GetHandle() != 0)
            return component->StopLocomotionAnimation;
        if ((uint64)component->IdleAnimation.GetHandle() != 0)
            return component->IdleAnimation;
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    default:
        break;
    }

    return component->Animation;
}

float LocomotionAnimInstance::GetCurrentStateDuration(const AnimationEvaluationContext& context) const
{
    const AnimationAsset* animation = ResolveAnimationAsset(context, SelectAnimationForState());
    return animation ? animation->m_DurationSeconds : 0.0f;
}

bool LocomotionAnimInstance::Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose)
{
    const AssetPtr<AnimationAsset> currentAnimation = SelectAnimationForState(m_State);
    if ((uint64)currentAnimation.GetHandle() == 0)
        return false;

    const bool bCurrentLooping = IsStateLooping(m_State);

    const float duration = GetCurrentStateDuration(context);
    if (!bCurrentLooping && duration > 0.0f)
        m_StatePlaybackTime = glm::min(m_StatePlaybackTime, duration);

    TArray<Mat4> currentPose;
    if (!EvaluateClip(
        context,
        currentAnimation,
        m_StatePlaybackTime,
        bCurrentLooping,
        currentPose))
    {
        return false;
    }

    if (!m_bTransitionActive)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const AssetPtr<AnimationAsset> previousAnimation = m_PreviousAnimation;
    if ((uint64)previousAnimation.GetHandle() == 0)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const bool bPreviousLooping = IsStateLooping(m_PreviousState);
    const AnimationAsset* previousResolved = ResolveAnimationAsset(context, previousAnimation);
    if (!previousResolved)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    float previousSampleTime = m_PreviousStatePlaybackTime;
    if (!bPreviousLooping && previousResolved->m_DurationSeconds > 0.0f)
        previousSampleTime = glm::min(previousSampleTime, previousResolved->m_DurationSeconds);

    TArray<Mat4> previousPose;
    if (!EvaluateClip(
            context,
            previousAnimation,
            previousSampleTime,
            bPreviousLooping,
            previousPose))
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const float alpha = ComputeTransitionBlendAlpha(m_TransitionElapsedTime, m_TransitionDuration);
    BlendLocalPoses(previousPose, currentPose, alpha, outLocalPose);
    return true;
}

const char* LocomotionAnimInstance::GetDebugStateName() const
{
    return ToDebugString(m_State);
}

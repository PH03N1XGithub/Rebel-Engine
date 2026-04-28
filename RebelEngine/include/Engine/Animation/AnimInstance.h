#pragma once

#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Assets/AssetPtr.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Gameplay/Framework/LocomotionTypes.h"

class AssetManager;
struct SkeletonAsset;
struct SkeletalMeshComponent;

enum class AnimationMovementMode : uint8
{
    None = 0,
    Walking,
    Falling,
    Flying,
    Custom
};

struct AnimationLocomotionState
{
    Vector3 WorldVelocity = Vector3(0.0f);
    Vector3 WorldAcceleration = Vector3(0.0f);
    Vector3 WorldMoveInput = Vector3(0.0f);
    Vector2 LocalMoveDirection = Vector2(0.0f);

    float HorizontalSpeed = 0.0f;
    float VerticalSpeed = 0.0f;
    float GroundDistance = 0.0f;
    float TimeInAir = 0.0f;
    float ActorYawDegrees = 0.0f;
    float ControllerYawDegrees = 0.0f;
    int JumpCount = 0;

    bool bIsGrounded = false;
    bool bHasMovementInput = false;
    bool bIsMoving = false;
    bool bJumpStarted = false;
    bool bLanded = false;
    bool bHasControllerYaw = false;

    AnimationMovementMode MovementMode = AnimationMovementMode::None;
    EGait Gait = EGait::Walk;
    EStance Stance = EStance::Standing;
    ERotationMode RotationMode = ERotationMode::VelocityDirection;
    ELocomotionAction Action = ELocomotionAction::None;
    ELocomotionState SemanticLocomotionState = ELocomotionState::Grounded;
    float MoveDirectionLocalDegrees = 0.0f;
    float VelocityYawDelta = 0.0f;
    float AimYawDelta = 0.0f;
};

struct AnimationEvaluationContext
{
    AssetManager* AssetManager = nullptr;
    const SkeletonAsset* Skeleton = nullptr;
    const TArray<Mat4>* LocalBindPose = nullptr;
};

class AnimInstance
{
    REFLECTABLE_CLASS(AnimInstance, void)

public:
    virtual ~AnimInstance() = default;

    void Initialize(SkeletalMeshComponent* owningComponent);

    SkeletalMeshComponent* GetOwningComponent() const { return m_OwningComponent; }
    const AnimationLocomotionState& GetLocomotionState() const { return m_LocomotionState; }
    void SetRootMotionEnabled(bool enabled) { m_bRootMotionEnabled = enabled; }
    bool IsRootMotionEnabled() const { return m_bRootMotionEnabled; }

    virtual void Update(float dt, const AnimationLocomotionState& locomotionState) = 0;
    virtual bool Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose) = 0;

    virtual const char* GetDebugStateName() const = 0;
    virtual float GetDebugPlaybackTime() const = 0;

protected:
    virtual void OnInitialize() {}

    const AnimationAsset* ResolveAnimationAsset(const AnimationEvaluationContext& context, const AssetPtr<AnimationAsset>& asset) const;
    bool EvaluateClip(
        const AnimationEvaluationContext& context,
        const AssetPtr<AnimationAsset>& asset,
        float playbackTime,
        bool bLooping,
        TArray<Mat4>& outLocalPose) const;

protected:
    SkeletalMeshComponent* m_OwningComponent = nullptr;
    AnimationLocomotionState m_LocomotionState{};
    bool m_bRootMotionEnabled = true;
};

enum class LocomotionAnimState : uint8
{
    Idle = 0,
    Move,
    Start,
    Stop,
    Pivot,
    InAir,
    Land
};

class LocomotionAnimInstance final : public AnimInstance
{
    REFLECTABLE_CLASS(LocomotionAnimInstance, AnimInstance)

public:
    void Update(float dt, const AnimationLocomotionState& locomotionState) override;
    bool Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose) override;

    const char* GetDebugStateName() const override;
    float GetDebugPlaybackTime() const override { return m_StatePlaybackTime; }

private:
    void TransitionTo(LocomotionAnimState newState);
    AssetPtr<AnimationAsset> SelectAnimationForState(LocomotionAnimState state) const;
    AssetPtr<AnimationAsset> SelectAnimationForState() const { return SelectAnimationForState(m_State); }
    float GetCurrentStateDuration(const AnimationEvaluationContext& context) const;
    static bool IsStateLooping(LocomotionAnimState state);
    bool IsMoving() const { return bIsMoving; }
    bool IsInAir() const { return bIsInAir; }
    bool IsStarting() const { return Action == ELocomotionAction::Starting; }
    bool IsPivoting() const { return Action == ELocomotionAction::Pivoting; }

private:
    float Speed = 0.0f;
    float VerticalSpeed = 0.0f;
    float GroundDistance = 0.0f;
    float TimeInAir = 0.0f;
    float ActorYawDegrees = 0.0f;
    float ControllerYawDegrees = 0.0f;
    float MoveDirectionLocal = 0.0f;
    float VelocityYawDelta = 0.0f;
    float AimYawDelta = 0.0f;
    int32 JumpCount = 0;
    bool IsGrounded = false;
    bool bIsInAir = false;
    bool HasMovementInput = false;
    bool bIsMoving = false;
    bool bIsStarting = false;
    bool bIsStopping = false;
    bool bIsPivoting = false;
    bool JumpStarted = false;
    bool Landed = false;
    bool HasControllerYaw = false;
    EGait Gait = EGait::Walk;
    EStance Stance = EStance::Standing;
    ERotationMode RotationMode = ERotationMode::VelocityDirection;
    ELocomotionAction Action = ELocomotionAction::None;
    ELocomotionState LocomotionState = ELocomotionState::Grounded;

    LocomotionAnimState m_State = LocomotionAnimState::Idle;
    LocomotionAnimState m_PreviousState = LocomotionAnimState::Idle;
    AssetPtr<AnimationAsset> m_PreviousAnimation{};
    float m_StateElapsedTime = 0.0f;
    float m_StatePlaybackTime = 0.0f;
    float m_PreviousStatePlaybackTime = 0.0f;
    float m_TransitionElapsedTime = 0.0f;
    float m_TransitionDuration = 0.15f;
    bool m_bTransitionActive = false;
    float m_StartMinDuration = 0.08f;
    float m_StopMinDuration = 0.08f;
    float m_PivotMinDuration = 0.08f;
    float m_LandMinDuration = 0.12f;
};

REFLECT_ABSTRACT_CLASS(AnimInstance, void)

REFLECT_CLASS(LocomotionAnimInstance, AnimInstance)
{
    REFLECT_PROPERTY(LocomotionAnimInstance, Speed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, VerticalSpeed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, GroundDistance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, TimeInAir,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, ActorYawDegrees,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, ControllerYawDegrees,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, MoveDirectionLocal,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, VelocityYawDelta,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, AimYawDelta,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, JumpCount,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, IsGrounded,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, bIsInAir,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, HasMovementInput,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, bIsMoving,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, bIsStarting,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, bIsStopping,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, bIsPivoting,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, JumpStarted,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, Landed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, HasControllerYaw,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, Gait,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, Stance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, RotationMode,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, Action,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, LocomotionState,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_State,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_PreviousState,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_StateElapsedTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_StatePlaybackTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_PreviousStatePlaybackTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_TransitionElapsedTime,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_TransitionDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_bTransitionActive,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_StartMinDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_StopMinDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_PivotMinDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LocomotionAnimInstance, m_LandMinDuration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
}
END_REFLECT_CLASS(LocomotionAnimInstance)

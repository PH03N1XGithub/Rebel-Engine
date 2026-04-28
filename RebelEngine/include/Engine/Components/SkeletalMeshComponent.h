#pragma once

#include <memory>
#include <utility>

#include "Engine/Animation/AnimInstance.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Assets/AssetPtr.h"
#include "Engine/Framework/TSubclassOf.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Rendering/Mesh.h"

struct AnimStateMachineRuntime
{
    uint64 StateMachineID = 0;
    uint64 CurrentStateID = 0;
    uint64 PreviousStateID = 0;
    float StateTime = 0.0f;
    float PreviousStateTime = 0.0f;
    float TransitionTime = 0.0f;
    float TransitionDuration = 0.0f;
    bool bTransitionActive = false;
};

struct AnimBlendNodeRuntime
{
    uint64 ScopeID = 0;
    uint64 NodeID = 0;
    float CurrentAlpha = 0.0f;
    bool bInitialized = false;
};

enum class ERootMotionRootLock : uint8
{
    RefPose,
    AnimFirstFrame,
    Zero
};

REFLECT_ENUM(ERootMotionRootLock)
    ENUM_OPTION(RefPose)
    ENUM_OPTION(AnimFirstFrame)
    ENUM_OPTION(Zero)
END_ENUM(ERootMotionRootLock)

enum class EVisualRootMotionStripMode : uint8
{
    None,
    StripTranslation,
    StripYaw,
    StripTranslationAndYaw
};

REFLECT_ENUM(EVisualRootMotionStripMode)
    ENUM_OPTION(None)
    ENUM_OPTION(StripTranslation)
    ENUM_OPTION(StripYaw)
    ENUM_OPTION(StripTranslationAndYaw)
END_ENUM(EVisualRootMotionStripMode)

struct SkeletalMeshComponent : SceneComponent
{
    AssetPtr<SkeletalMeshAsset> Mesh{};
    AssetPtr<AnimGraphAsset> AnimGraph{};
    Rebel::TSubclassOf<AnimInstance> AnimInstanceClass{};
    AssetPtr<AnimationAsset> Animation{};
    AssetPtr<AnimationAsset> IdleAnimation{};
    AssetPtr<AnimationAsset> MoveAnimation{};
    AssetPtr<AnimationAsset> StartAnimation{};
    AssetPtr<AnimationAsset> StopLocomotionAnimation{};
    AssetPtr<AnimationAsset> PivotAnimation{};
    AssetPtr<AnimationAsset> JumpAnimation{};
    AssetPtr<AnimationAsset> DoubleJumpAnimation{};
    AssetPtr<AnimationAsset> InAirAnimation{};
    AssetPtr<AnimationAsset> FallingAnimation{};
    AssetPtr<AnimationAsset> LandAnimation{};
    MaterialHandle Material{};

    Bool bIsVisible = true;
    Bool bCastShadows = true;
    Bool bDrawSkeleton = false;
    Bool bPlayAnimation = true;
    Bool bLoopAnimation = true;
    Bool bEnableRootMotion = false;
    ERootMotionRootLock RootMotionRootLock = ERootMotionRootLock::RefPose;
    EVisualRootMotionStripMode VisualRootMotionStripMode = EVisualRootMotionStripMode::StripTranslation;
    Float PlaybackSpeed = 1.0f;
    Float PlaybackTime = 0.0f;
    AnimationLocomotionState LocomotionState{};

    TArray<Mat4> LocalPose;
    TArray<Mat4> GlobalPose;
    TArray<Mat4> FinalPalette;

    TArray<Vector3> RuntimeBoneLocalTranslations;
    TArray<Vector3> RuntimeBoneGlobalTranslations;
    TArray<Vector3> RuntimeBoneLocalScales;
    TArray<Vector3> RuntimeBoneGlobalScales;
    TArray<Quaternion> RuntimeBoneLocalRotations;
    TArray<Quaternion> RuntimeBoneGlobalRotations;
    TArray<AnimStateMachineRuntime> StateMachineRuntimes;
    TArray<AnimBlendNodeRuntime> BlendNodeRuntimes;
    std::unique_ptr<AnimInstance> AnimScriptInstance{};
    AssetPtr<AnimationAsset> OverrideAnimation{};
    Bool bOverrideAnimationActive = false;
    Bool bOverrideAnimationLooping = false;
    Float OverridePlaybackSpeed = 1.0f;
    Float OverridePlaybackTime = 0.0f;
    Bool bOverrideBlendActive = false;
    Float OverrideBlendDuration = 0.0f;
    Float OverrideBlendElapsed = 0.0f;
    TArray<Mat4> OverrideBlendSourcePose;
    Bool bOverrideLockRootBoneTranslation = false;
    Bool bOverrideBlendOutActive = false;
    Float OverrideBlendOutDuration = 0.08f;
    Float OverrideBlendOutElapsed = 0.0f;
    TArray<Mat4> OverrideBlendOutSourcePose;

    SkeletalMeshComponent() = default;

    SkeletalMeshComponent(AssetHandle meshAsset,
                          MaterialHandle mat = MaterialHandle(),
                          Bool visible = true,
                          Bool castShadows = true)
        : Mesh(meshAsset)
        , Material(mat)
        , bIsVisible(visible)
        , bCastShadows(castShadows)
    {}

    Bool IsValid() const
    {
        return (uint64)Mesh.GetHandle() != 0;
    }

    template<typename TAnimInstance, typename... Args>
    TAnimInstance& CreateAnimInstance(Args&&... args)
    {
        auto instance = std::make_unique<TAnimInstance>(std::forward<Args>(args)...);
        TAnimInstance* rawInstance = instance.get();
        SetAnimInstance(std::move(instance));
        return *rawInstance;
    }

    void SetAnimInstance(std::unique_ptr<AnimInstance> animInstance)
    {
        AnimScriptInstance = std::move(animInstance);
        if (AnimScriptInstance)
            AnimScriptInstance->Initialize(this);
    }

    AnimInstance* GetAnimInstance() const { return AnimScriptInstance.get(); }

    AnimStateMachineRuntime& GetStateMachineRuntime(uint64 stateMachineID)
    {
        for (AnimStateMachineRuntime& runtime : StateMachineRuntimes)
        {
            if (runtime.StateMachineID == stateMachineID)
                return runtime;
        }

        AnimStateMachineRuntime& runtime = StateMachineRuntimes.Emplace();
        runtime.StateMachineID = stateMachineID;
        return runtime;
    }

    AnimBlendNodeRuntime& GetBlendNodeRuntime(uint64 scopeID, uint64 nodeID)
    {
        for (AnimBlendNodeRuntime& runtime : BlendNodeRuntimes)
        {
            if (runtime.ScopeID == scopeID && runtime.NodeID == nodeID)
                return runtime;
        }

        AnimBlendNodeRuntime& runtime = BlendNodeRuntimes.Emplace();
        runtime.ScopeID = scopeID;
        runtime.NodeID = nodeID;
        return runtime;
    }

    AnimInstance* EnsureAnimInstance(const Rebel::Core::Reflection::TypeInfo* requestedType = nullptr)
    {
        const Rebel::Core::Reflection::TypeInfo* type = requestedType;
        if (!type)
            type = AnimInstanceClass.Get();
        if (!type)
            type = LocomotionAnimInstance::StaticType();

        if (AnimScriptInstance && AnimScriptInstance->GetType() == type)
            return AnimScriptInstance.get();

        if (!type->CreateInstance || !AnimInstance::StaticType() || !type->IsA(AnimInstance::StaticType()))
            type = LocomotionAnimInstance::StaticType();

        auto* raw = static_cast<AnimInstance*>(type->CreateInstance());
        SetAnimInstance(std::unique_ptr<AnimInstance>(raw));
        return AnimScriptInstance.get();
    }
    
    void PlayAnimation(
        const AssetPtr<AnimationAsset>& animation,
        Bool bLooping = false,
        Float playbackSpeed = 1.0f,
        Float blendDuration = 0.0f)
    {
        OverrideAnimation = animation;
        bOverrideAnimationActive = (uint64)OverrideAnimation.GetHandle() != 0;
        bOverrideAnimationLooping = bLooping;
        OverridePlaybackSpeed = glm::max(0.0f, playbackSpeed);
        OverridePlaybackTime = 0.0f;
        OverrideBlendDuration = glm::max(0.0f, blendDuration);
        OverrideBlendElapsed = 0.0f;
        bOverrideBlendActive = bOverrideAnimationActive && OverrideBlendDuration > 1.0e-6f;
        OverrideBlendSourcePose = LocalPose;
        bOverrideBlendOutActive = false;
        OverrideBlendOutElapsed = 0.0f;
        OverrideBlendOutSourcePose.Clear();
    }

    void PlayAnimation(
        const AssetHandle animationHandle,
        const Bool bLooping = false,
        const Float playbackSpeed = 1.0f,
        const Float blendDuration = 0.0f)
    {
        PlayAnimation(AssetPtr<AnimationAsset>(animationHandle), bLooping, playbackSpeed, blendDuration);
    }

    void StopAnimation()
    {
        const float blendOutDuration = glm::max(0.0f, OverrideBlendOutDuration);
        const bool bCanBlendOut = blendOutDuration > 1.0e-6f && !LocalPose.IsEmpty();

        bOverrideAnimationActive = false;
        OverrideAnimation = {};
        OverridePlaybackTime = 0.0f;
        bOverrideBlendActive = false;
        OverrideBlendDuration = 0.0f;
        OverrideBlendElapsed = 0.0f;
        OverrideBlendSourcePose.Clear();
        bOverrideLockRootBoneTranslation = false;

        bOverrideBlendOutActive = bCanBlendOut;
        OverrideBlendOutElapsed = 0.0f;
        if (bCanBlendOut)
            OverrideBlendOutSourcePose = LocalPose;
        else
            OverrideBlendOutSourcePose.Clear();
    }

    explicit operator Bool() const { return IsValid(); }

    REFLECTABLE_CLASS(SkeletalMeshComponent, SceneComponent)
};

REFLECT_CLASS(SkeletalMeshComponent, SceneComponent)
{
    REFLECT_PROPERTY(SkeletalMeshComponent, Mesh,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, AnimGraph,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, AnimInstanceClass,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Animation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, IdleAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, MoveAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, StartAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, StopLocomotionAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PivotAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, JumpAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, DoubleJumpAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, InAirAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, FallingAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, LandAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Material,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bIsVisible,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bCastShadows,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bDrawSkeleton,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bPlayAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bLoopAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bEnableRootMotion,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, RootMotionRootLock,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, VisualRootMotionStripMode,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackSpeed,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackTime,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable | EPropertyFlags::Transient);

    REFLECT_PROPERTY(SkeletalMeshComponent, OverrideBlendOutDuration,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(SkeletalMeshComponent)
REFLECT_ECS_COMPONENT(SkeletalMeshComponent)

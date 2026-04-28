#include "Engine/Framework/EnginePch.h"
#include "Engine/Animation/AnimationModule.h"

#include <cmath>
#include <unordered_set>

#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimInstance.h"
#include "Engine/Animation/AnimationDebug.h"
#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Animation/SkeletonAsset.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Components/Components.h"
#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Gameplay/Framework/LocomotionCharacter.h"
#include "Engine/Scene/Scene.h"

namespace
{
DEFINE_LOG_CATEGORY(AnimationModuleLog)

struct AnimationEvaluationTrace
{
    const char* Path = "BindPose";
    String ClipSummary{};
    const AnimationAsset* PrimaryAnimation = nullptr;
    float PlaybackTime = 0.0f;
};

struct RootMotionBoneDiagnostic
{
    int32 BoneIndex = -1;
    float YawBeforeDegrees = 0.0f;
    float YawAfterDegrees = 0.0f;
    float LocalTranslationDelta = 0.0f;
};

void AppendClipSummary(AnimationEvaluationTrace* trace, const AnimationAsset* animation)
{
    if (!trace || !animation || animation->m_ClipName.length() == 0)
        return;

    const std::string currentSummary = trace->ClipSummary.c_str();
    const std::string clipName = animation->m_ClipName.c_str();
    if (currentSummary.find(clipName) != std::string::npos)
        return;

    if (trace->ClipSummary.length() > 0)
        trace->ClipSummary += ", ";

    trace->ClipSummary += animation->m_ClipName;
    if (!trace->PrimaryAnimation)
        trace->PrimaryAnimation = animation;
}

const char* ToString(const ERootMotionRootLock mode)
{
    switch (mode)
    {
    case ERootMotionRootLock::RefPose: return "RefPose";
    case ERootMotionRootLock::AnimFirstFrame: return "AnimFirstFrame";
    case ERootMotionRootLock::Zero: return "Zero";
    default: return "Unknown";
    }
}

const char* ToString(const EVisualRootMotionStripMode mode)
{
    switch (mode)
    {
    case EVisualRootMotionStripMode::None: return "None";
    case EVisualRootMotionStripMode::StripTranslation: return "StripTranslation";
    case EVisualRootMotionStripMode::StripYaw: return "StripYaw";
    case EVisualRootMotionStripMode::StripTranslationAndYaw: return "StripTranslationAndYaw";
    default: return "Unknown";
    }
}

float ComputeSignedHorizontalAngleDegrees(const Vector3& from, const Vector3& to)
{
    Vector3 fromPlanar(from.x, from.y, 0.0f);
    Vector3 toPlanar(to.x, to.y, 0.0f);
    const float fromLen = glm::length(fromPlanar);
    const float toLen = glm::length(toPlanar);
    if (fromLen <= 1.0e-6f || toLen <= 1.0e-6f)
        return 0.0f;

    fromPlanar /= fromLen;
    toPlanar /= toLen;

    const float dot = FMath::clamp(glm::dot(fromPlanar, toPlanar), -1.0f, 1.0f);
    const float crossZ = glm::cross(fromPlanar, toPlanar).z;
    return glm::degrees(std::atan2(crossZ, dot));
}

float ComputeLocalTranslationDelta(const Vector3& bindTranslation, const Vector3& currentTranslation)
{
    const Vector3 delta = currentTranslation - bindTranslation;
    return glm::length(delta);
}

float ComputeBoneCarrierScore(
    const float yawDegrees,
    const float translationDelta,
    const bool bNamedCarrier,
    const int32 depth)
{
    float score = std::fabs(yawDegrees) + translationDelta * 25.0f;
    if (bNamedCarrier)
        score += 45.0f;
    if (depth <= 1)
        score += 20.0f;
    else if (depth == 2)
        score += 10.0f;
    return score;
}

int32 GetBoneDepth(const SkeletonAsset* skeleton, int32 boneIndex)
{
    if (!skeleton || boneIndex < 0 || boneIndex >= skeleton->m_Parent.Num())
        return 0;

    int32 depth = 0;
    int32 cursor = boneIndex;
    while (cursor >= 0 && cursor < skeleton->m_Parent.Num())
    {
        cursor = skeleton->m_Parent[cursor];
        if (cursor >= 0)
            ++depth;
    }
    return depth;
}

void ClearAnimationRuntimeData(SkeletalMeshComponent* skComp)
{
    if (!skComp)
        return;

    skComp->LocalPose.Clear();
    skComp->GlobalPose.Clear();
    skComp->FinalPalette.Clear();

    skComp->RuntimeBoneLocalTranslations.Clear();
    skComp->RuntimeBoneGlobalTranslations.Clear();
    skComp->RuntimeBoneLocalScales.Clear();
    skComp->RuntimeBoneGlobalScales.Clear();
    skComp->RuntimeBoneLocalRotations.Clear();
    skComp->RuntimeBoneGlobalRotations.Clear();
}

void UpdateRuntimeBoneTransforms(
    SkeletalMeshComponent* skComp,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localPose,
    const TArray<Mat4>& globalPose)
{
    if (!skComp || !skeleton)
        return;

    const int32 boneCount = static_cast<int32>(skeleton->m_InvBind.Num());
    if (boneCount <= 0 || localPose.Num() != boneCount || globalPose.Num() != boneCount)
    {
        skComp->RuntimeBoneLocalTranslations.Clear();
        skComp->RuntimeBoneGlobalTranslations.Clear();
        skComp->RuntimeBoneLocalScales.Clear();
        skComp->RuntimeBoneGlobalScales.Clear();
        skComp->RuntimeBoneLocalRotations.Clear();
        skComp->RuntimeBoneGlobalRotations.Clear();
        return;
    }

    skComp->RuntimeBoneLocalTranslations.Resize(boneCount);
    skComp->RuntimeBoneGlobalTranslations.Resize(boneCount);
    skComp->RuntimeBoneLocalScales.Resize(boneCount);
    skComp->RuntimeBoneGlobalScales.Resize(boneCount);
    skComp->RuntimeBoneLocalRotations.Resize(boneCount);
    skComp->RuntimeBoneGlobalRotations.Resize(boneCount);

    for (int32 i = 0; i < boneCount; ++i)
    {
        Vector3 localT(0.0f);
        Vector3 localS(1.0f);
        Quaternion localR(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localPose[i], localT, localR, localS);

        Vector3 globalT(0.0f);
        Vector3 globalS(1.0f);
        Quaternion globalR(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(globalPose[i], globalT, globalR, globalS);

        skComp->RuntimeBoneLocalTranslations[i] = localT;
        skComp->RuntimeBoneGlobalTranslations[i] = globalT;
        skComp->RuntimeBoneLocalScales[i] = localS;
        skComp->RuntimeBoneGlobalScales[i] = globalS;
        skComp->RuntimeBoneLocalRotations[i] = localR;
        skComp->RuntimeBoneGlobalRotations[i] = globalR;
    }
}

void ValidateBindPoseMatricesOnce(const SkeletonAsset* skeleton, const TArray<Mat4>& globalBindPose)
{
#if ANIMATION_DEBUG
    if (!skeleton)
        return;

    static std::unordered_set<uint64> validatedSkeletons;
    const uint64 skeletonID = (uint64)skeleton->ID;
    if (validatedSkeletons.find(skeletonID) != validatedSkeletons.end())
        return;

    int32 worstBoneIndex = -1;
    const float worstError =
        AnimationRuntime::ValidateBindPoseIdentity(skeleton, globalBindPose, &worstBoneIndex);

    if (worstError > 1e-3f)
    {
        String boneName = "Unknown";
        if (worstBoneIndex >= 0 && worstBoneIndex < skeleton->m_BoneNames.Num())
            boneName = skeleton->m_BoneNames[worstBoneIndex];

        ANIMATION_DEBUG_LOG("[Skinning][BindPose] WARNING skeleton="
                            << (uint64)skeleton->ID
                            << " worstError=" << worstError
                            << " boneIndex=" << worstBoneIndex
                            << " boneName=" << boneName.c_str());
    }
    else
    {
        ANIMATION_DEBUG_LOG("[Skinning][BindPose] OK skeleton="
                            << (uint64)skeleton->ID
                            << " maxError=" << worstError);
    }

    ANIMATION_DEBUG_FLUSH();
    validatedSkeletons.insert(skeletonID);
#else
    (void)skeleton;
    (void)globalBindPose;
#endif
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

const Rebel::Core::Reflection::PropertyInfo* FindAnimInstanceProperty(
    const AnimInstance* animInstance,
    const String& propertyName)
{
    if (!animInstance || propertyName.length() == 0)
        return nullptr;

    const Rebel::Core::Reflection::TypeInfo* type = animInstance->GetType();
    while (type)
    {
        for (const Rebel::Core::Reflection::PropertyInfo& prop : type->Properties)
        {
            if (prop.Name == propertyName)
                return &prop;
        }
        type = type->Super;
    }

    return nullptr;
}

uint64 MakeStatePoseGraphRuntimeScopeID(const uint64 stateMachineID, const uint64 stateID)
{
    return (stateMachineID << 32) ^ stateID;
}

float ResolveBlendAlphaTarget(const AnimGraphNode& node, const AnimInstance* animInstance)
{
    const float fallbackAlpha = FMath::clamp(node.BlendAlpha, 0.0f, 1.0f);
    using Rebel::Core::Reflection::EPropertyType;

    const Rebel::Core::Reflection::PropertyInfo* prop = FindAnimInstanceProperty(animInstance, node.BlendParameterName);
    if (!prop)
        return fallbackAlpha;

    const uint8* base = reinterpret_cast<const uint8*>(animInstance);
    const void* valuePtr = base + prop->Offset;
    switch (node.BlendAlphaMode)
    {
    case AnimBlendAlphaMode::FloatProperty:
    {
        if (prop->Type != EPropertyType::Float)
            return fallbackAlpha;

        const float range = node.BlendInputMax - node.BlendInputMin;
        if (std::fabs(range) <= 1.0e-6f)
            return fallbackAlpha;

        const float value = *reinterpret_cast<const float*>(valuePtr);
        return FMath::clamp((value - node.BlendInputMin) / range, 0.0f, 1.0f);
    }
    case AnimBlendAlphaMode::BoolProperty:
    {
        if (prop->Type != EPropertyType::Bool)
            return fallbackAlpha;

        bool value = *reinterpret_cast<const bool*>(valuePtr);
        if (node.bBlendInvertBool)
            value = !value;
        return value ? 1.0f : 0.0f;
    }
    case AnimBlendAlphaMode::Fixed:
    default:
        return fallbackAlpha;
    }
}

float ResolveBlendAlpha(
    SkeletalMeshComponent* skComp,
    const uint64 runtimeScopeID,
    const AnimGraphNode& node,
    const AnimInstance* animInstance,
    const float dt)
{
    const float targetAlpha = ResolveBlendAlphaTarget(node, animInstance);
    const float blendTime = FMath::max(0.0f, node.BlendTime);
    if (!skComp || blendTime <= 1.0e-6f || dt <= 0.0f)
        return targetAlpha;

    AnimBlendNodeRuntime& runtime = skComp->GetBlendNodeRuntime(runtimeScopeID, node.ID);
    if (!runtime.bInitialized)
    {
        runtime.CurrentAlpha = targetAlpha;
        runtime.bInitialized = true;
        return runtime.CurrentAlpha;
    }

    const float delta = targetAlpha - runtime.CurrentAlpha;
    const float maxStep = dt / blendTime;
    if (std::fabs(delta) <= maxStep)
        runtime.CurrentAlpha = targetAlpha;
    else
        runtime.CurrentAlpha += delta > 0.0f ? maxStep : -maxStep;

    runtime.CurrentAlpha = FMath::clamp(runtime.CurrentAlpha, 0.0f, 1.0f);
    return runtime.CurrentAlpha;
}

int32 ReadEnumPropertyValueAsInt(
    const Rebel::Core::Reflection::PropertyInfo& prop,
    const void* valuePtr)
{
    if (!valuePtr)
        return 0;

    switch (prop.Size)
    {
    case sizeof(uint8):
        return static_cast<int32>(*reinterpret_cast<const uint8*>(valuePtr));
    case sizeof(uint16):
        return static_cast<int32>(*reinterpret_cast<const uint16*>(valuePtr));
    case sizeof(uint32):
        return static_cast<int32>(*reinterpret_cast<const uint32*>(valuePtr));
    case sizeof(uint64):
        return static_cast<int32>(*reinterpret_cast<const uint64*>(valuePtr));
    default:
        return 0;
    }
}

float GetPoseGraphClipDuration(
    AssetManager& assetManager,
    const AnimGraphAsset& graph,
    const AnimPoseGraph& poseGraph,
    uint64 nodeID,
    TArray<uint64>& visited)
{
    if (nodeID == 0)
        return 0.0f;
    for (const uint64 visitedID : visited)
    {
        if (visitedID == nodeID)
            return 0.0f;
    }
    visited.Add(nodeID);

    const AnimGraphNode* node = graph.FindNode(poseGraph, nodeID);
    if (!node)
        return 0.0f;

    if (node->Kind == AnimGraphNodeKind::AnimationClip && IsValidAssetHandle(node->AnimationClip))
    {
        const AnimationAsset* animation = dynamic_cast<const AnimationAsset*>(assetManager.Load(node->AnimationClip));
        if (!animation)
            return 0.0f;

        const float clipStartTime = FMath::max(0.0f, node->ClipStartTime);
        return FMath::max(0.0f, animation->m_DurationSeconds - clipStartTime);
    }

    if (node->Kind == AnimGraphNodeKind::Blend)
    {
        const float durationA = GetPoseGraphClipDuration(assetManager, graph, poseGraph, node->InputA, visited);
        const float durationB = GetPoseGraphClipDuration(assetManager, graph, poseGraph, node->InputB, visited);
        return FMath::max(durationA, durationB);
    }

    if (node->Kind == AnimGraphNodeKind::Output)
        return GetPoseGraphClipDuration(assetManager, graph, poseGraph, node->InputPose, visited);

    return 0.0f;
}

bool EvaluateStatePoseGraphNode(
    SkeletalMeshComponent* skComp,
    AnimInstance* animInstance,
    AssetManager& assetManager,
    const AnimGraphAsset& graph,
    const AnimPoseGraph& poseGraph,
    const AnimGraphNode& node,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    const float playbackTime,
    const float dt,
    const bool bApplyRootMotion,
    TArray<Mat4>& outLocalPose,
    TArray<uint64>& visited,
    const bool bLooping,
    const uint64 runtimeScopeID,
    AnimationEvaluationTrace* trace)
{
    for (const uint64 visitedID : visited)
    {
        if (visitedID == node.ID)
            return false;
    }
    visited.Add(node.ID);

    switch (node.Kind)
    {
    case AnimGraphNodeKind::AnimationClip:
    {
        AnimationAsset* animation = nullptr;
        if (IsValidAssetHandle(node.AnimationClip))
            animation = dynamic_cast<AnimationAsset*>(assetManager.Load(node.AnimationClip));

        if (animation &&
            (uint64)animation->m_SkeletonID != 0 &&
            (uint64)animation->m_SkeletonID != (uint64)skeleton->ID)
        {
            animation = nullptr;
        }

        const float sampleTime = animation
            ? AnimationRuntime::NormalizePlaybackTime(
                playbackTime + FMath::max(0.0f, node.ClipStartTime),
                animation->m_DurationSeconds,
                bLooping)
            : 0.0f;

        AppendClipSummary(trace, animation);

        return AnimationRuntime::EvaluateLocalPose(
            skeleton,
            animation,
            sampleTime,
            localBindPose,
            outLocalPose,
            bApplyRootMotion);
    }
    case AnimGraphNodeKind::Blend:
    {
        const AnimGraphNode* inputA = graph.FindNode(poseGraph, node.InputA);
        const AnimGraphNode* inputB = graph.FindNode(poseGraph, node.InputB);
        if (!inputA && !inputB)
            return false;

        TArray<Mat4> poseA = localBindPose;
        TArray<Mat4> poseB = localBindPose;
        TArray<uint64> visitedA = visited;
        TArray<uint64> visitedB = visited;
        const bool hasA = inputA && EvaluateStatePoseGraphNode(
            skComp, animInstance, assetManager, graph, poseGraph, *inputA, skeleton, localBindPose, playbackTime, dt, bApplyRootMotion, poseA, visitedA, bLooping, runtimeScopeID, trace);
        const bool hasB = inputB && EvaluateStatePoseGraphNode(
            skComp, animInstance, assetManager, graph, poseGraph, *inputB, skeleton, localBindPose, playbackTime, dt, bApplyRootMotion, poseB, visitedB, bLooping, runtimeScopeID, trace);

        if (hasA && hasB)
        {
            BlendLocalPoses(poseA, poseB, ResolveBlendAlpha(skComp, runtimeScopeID, node, animInstance, dt), outLocalPose);
            return true;
        }

        outLocalPose = hasA ? poseA : poseB;
        return hasA || hasB;
    }
    case AnimGraphNodeKind::Output:
    {
        const AnimGraphNode* input = graph.FindNode(poseGraph, node.InputPose);
        if (!input)
            return false;

        return EvaluateStatePoseGraphNode(
            skComp,
            animInstance,
            assetManager,
            graph,
            poseGraph,
            *input,
            skeleton,
            localBindPose,
            playbackTime,
            dt,
            bApplyRootMotion,
            outLocalPose,
            visited,
            bLooping,
            runtimeScopeID,
            trace);
    }
    default:
        return false;
    }
}

bool EvaluateStatePoseGraph(
    SkeletalMeshComponent* skComp,
    AnimInstance* animInstance,
    AssetManager& assetManager,
    const AnimGraphAsset& graph,
    const AnimState& state,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    const float playbackTime,
    const float dt,
    const bool bApplyRootMotion,
    TArray<Mat4>& outLocalPose,
    TArray<uint64>& visited,
    const uint64 runtimeScopeID,
    AnimationEvaluationTrace* trace)
{
    const AnimGraphNode* output = graph.FindOutputNode(state.StateGraph);
    if (!output)
        return false;

    return EvaluateStatePoseGraphNode(
        skComp,
        animInstance,
        assetManager,
        graph,
        state.StateGraph,
        *output,
        skeleton,
        localBindPose,
        playbackTime,
        dt,
        bApplyRootMotion,
        outLocalPose,
        visited,
        state.bLoop,
        runtimeScopeID,
        trace);
}

bool EvaluateAnimGraphNode(
    SkeletalMeshComponent* skComp,
    AnimInstance* animInstance,
    AssetManager& assetManager,
    const AnimGraphAsset& graph,
    const AnimGraphNode& node,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    const float playbackTime,
    const float dt,
    const bool bApplyRootMotion,
    TArray<Mat4>& outLocalPose,
    TArray<uint64>& visited,
    const bool bLooping = true,
    const uint64 runtimeScopeID = 0,
    AnimationEvaluationTrace* trace = nullptr)
{
    for (uint64 visitedID : visited)
    {
        if (visitedID == node.ID)
            return false;
    }
    visited.Add(node.ID);

    switch (node.Kind)
    {
    case AnimGraphNodeKind::AnimationClip:
    {
        AnimationAsset* animation = nullptr;
        if (IsValidAssetHandle(node.AnimationClip))
            animation = dynamic_cast<AnimationAsset*>(assetManager.Load(node.AnimationClip));

        if (animation &&
            (uint64)animation->m_SkeletonID != 0 &&
            (uint64)animation->m_SkeletonID != (uint64)skeleton->ID)
        {
            animation = nullptr;
        }

        const float sampleTime = animation
            ? AnimationRuntime::NormalizePlaybackTime(
                playbackTime + FMath::max(0.0f, node.ClipStartTime),
                animation->m_DurationSeconds,
                bLooping)
            : 0.0f;

        AppendClipSummary(trace, animation);

        return AnimationRuntime::EvaluateLocalPose(
            skeleton,
            animation,
            sampleTime,
            localBindPose,
            outLocalPose,
            bApplyRootMotion);
    }
    case AnimGraphNodeKind::Blend:
    {
        const AnimGraphNode* inputA = graph.FindNode(node.InputA);
        const AnimGraphNode* inputB = graph.FindNode(node.InputB);
        if (!inputA && !inputB)
            return false;

        TArray<Mat4> poseA = localBindPose;
        TArray<Mat4> poseB = localBindPose;
        TArray<uint64> visitedA = visited;
        TArray<uint64> visitedB = visited;
        const bool hasA = inputA && EvaluateAnimGraphNode(
            skComp, animInstance, assetManager, graph, *inputA, skeleton, localBindPose, playbackTime, dt, bApplyRootMotion, poseA, visitedA, bLooping, runtimeScopeID, trace);
        const bool hasB = inputB && EvaluateAnimGraphNode(
            skComp, animInstance, assetManager, graph, *inputB, skeleton, localBindPose, playbackTime, dt, bApplyRootMotion, poseB, visitedB, bLooping, runtimeScopeID, trace);

        if (hasA && hasB)
        {
            BlendLocalPoses(poseA, poseB, ResolveBlendAlpha(skComp, runtimeScopeID, node, animInstance, dt), outLocalPose);
            return true;
        }

        outLocalPose = hasA ? poseA : poseB;
        return hasA || hasB;
    }
    case AnimGraphNodeKind::Output:
    {
        const AnimGraphNode* input = graph.FindNode(node.InputPose);
        if (!input)
            return false;

        return EvaluateAnimGraphNode(
            skComp,
            animInstance,
            assetManager,
            graph,
            *input,
            skeleton,
            localBindPose,
            playbackTime,
            dt,
            bApplyRootMotion,
            outLocalPose,
            visited,
            bLooping,
            runtimeScopeID,
            trace);
    }
    case AnimGraphNodeKind::StateMachine:
    {
        if (!skComp)
            return false;

        const AnimStateMachine* stateMachine = graph.FindStateMachine(node.StateMachineID);
        if (!stateMachine || stateMachine->States.IsEmpty())
            return false;

        auto findState = [&](uint64 stateID) -> const AnimState*
        {
            for (const AnimState& state : stateMachine->States)
            {
                if (state.ID == stateID)
                    return &state;
            }
            return nullptr;
        };

        auto getStateClipDuration = [&](const AnimState& state) -> float
        {
            TArray<uint64> visited;
            const AnimGraphNode* output = graph.FindOutputNode(state.StateGraph);
            return output ? GetPoseGraphClipDuration(assetManager, graph, state.StateGraph, output->InputPose, visited) : 0.0f;
        };

        const Rebel::Core::Reflection::TypeInfo* instanceType = animInstance ? animInstance->GetType() : nullptr;
        auto findProperty = [&](const String& name) -> const Rebel::Core::Reflection::PropertyInfo*
        {
            const Rebel::Core::Reflection::TypeInfo* type = instanceType;
            while (type)
            {
                for (const Rebel::Core::Reflection::PropertyInfo& prop : type->Properties)
                {
                    if (prop.Name == name)
                        return &prop;
                }
                type = type->Super;
            }
            return nullptr;
        };

        AnimStateMachineRuntime& runtime = skComp->GetStateMachineRuntime(stateMachine->ID);
        if (!findState(runtime.CurrentStateID))
        {
            runtime.CurrentStateID = stateMachine->EntryStateID != 0 ? stateMachine->EntryStateID : stateMachine->States[0].ID;
            runtime.PreviousStateID = 0;
            runtime.StateTime = 0.0f;
            runtime.PreviousStateTime = 0.0f;
            runtime.TransitionTime = 0.0f;
            runtime.TransitionDuration = 0.0f;
            runtime.bTransitionActive = false;
        }

        auto conditionPasses = [&](const AnimTransitionCondition& condition) -> bool
        {
            const AnimState* currentStateForTiming = findState(runtime.CurrentStateID);
            const float stateTime = runtime.StateTime;
            const float stateDuration = currentStateForTiming ? getStateClipDuration(*currentStateForTiming) : 0.0f;
            const float remainingTime = stateDuration > 1.0e-6f ? FMath::max(0.0f, stateDuration - stateTime) : 0.0f;
            const float remainingRatio = stateDuration > 1.0e-6f ? remainingTime / stateDuration : 0.0f;

            switch (condition.Op)
            {
            case AnimConditionOp::StateTimeGreater:
                return stateTime > condition.FloatValue;
            case AnimConditionOp::StateTimeLess:
                return stateTime < condition.FloatValue;
            case AnimConditionOp::AnimTimeRemainingLess:
                return stateDuration > 1.0e-6f && remainingTime < condition.FloatValue;
            case AnimConditionOp::AnimTimeRemainingRatioLess:
                return stateDuration > 1.0e-6f && remainingRatio < condition.FloatValue;
            default:
                break;
            }

            if (!animInstance)
                return false;

            const Rebel::Core::Reflection::PropertyInfo* prop = findProperty(condition.PropertyName);
            if (!prop)
                return false;

            const uint8* base = reinterpret_cast<const uint8*>(animInstance);
            const void* valuePtr = base + prop->Offset;
            using Rebel::Core::Reflection::EPropertyType;
            switch (condition.Op)
            {
            case AnimConditionOp::BoolIsTrue:
                return prop->Type == EPropertyType::Bool && *reinterpret_cast<const bool*>(valuePtr);
            case AnimConditionOp::BoolIsFalse:
                return prop->Type == EPropertyType::Bool && !*reinterpret_cast<const bool*>(valuePtr);
            case AnimConditionOp::FloatGreater:
                return prop->Type == EPropertyType::Float && *reinterpret_cast<const float*>(valuePtr) > condition.FloatValue;
            case AnimConditionOp::FloatLess:
                return prop->Type == EPropertyType::Float && *reinterpret_cast<const float*>(valuePtr) < condition.FloatValue;
            case AnimConditionOp::IntGreater:
                return prop->Type == EPropertyType::Int32 && *reinterpret_cast<const int32*>(valuePtr) > condition.IntValue;
            case AnimConditionOp::IntLess:
                return prop->Type == EPropertyType::Int32 && *reinterpret_cast<const int32*>(valuePtr) < condition.IntValue;
            case AnimConditionOp::EnumEquals:
            case AnimConditionOp::EnumNotEquals:
            {
                if (prop->Type != EPropertyType::Enum)
                    return false;

                const int32 enumValue = ReadEnumPropertyValueAsInt(*prop, valuePtr);
                const bool bEquals = enumValue == condition.IntValue;
                return condition.Op == AnimConditionOp::EnumEquals ? bEquals : !bEquals;
            }
            default:
                return false;
            }
        };

        auto conditionsPass = [&](const TArray<AnimTransitionCondition>& conditions) -> bool
        {
            if (conditions.IsEmpty())
                return true;

            for (const AnimTransitionCondition& condition : conditions)
            {
                if (!conditionPasses(condition))
                    return false;
            }
            return true;
        };

        auto transitionPasses = [&](const AnimTransition& transition) -> bool
        {
            return conditionsPass(transition.Conditions);
        };

        if (runtime.bTransitionActive)
        {
            runtime.TransitionTime += dt;
            if (runtime.TransitionTime >= runtime.TransitionDuration)
            {
                runtime.bTransitionActive = false;
                runtime.PreviousStateID = 0;
                runtime.PreviousStateTime = 0.0f;
                runtime.TransitionTime = 0.0f;
            }
        }
        else
        {
            bool didAliasTransition = false;
            for (const AnimStateAlias& alias : stateMachine->Aliases)
            {
                bool allowedFromCurrent = alias.bGlobalAlias;
                if (!allowedFromCurrent)
                {
                    for (const uint64 allowedStateID : alias.AllowedFromStateIDs)
                    {
                        if (allowedStateID == runtime.CurrentStateID)
                        {
                            allowedFromCurrent = true;
                            break;
                        }
                    }
                }

                if (!allowedFromCurrent || !conditionsPass(alias.Conditions))
                    continue;

                auto tryAliasTarget = [&](const uint64 targetStateID, const float blendDuration, const TArray<AnimTransitionCondition>* targetConditions) -> bool
                {
                    if (targetStateID == 0 || targetStateID == runtime.CurrentStateID || !findState(targetStateID))
                        return false;
                    if (targetConditions && !conditionsPass(*targetConditions))
                        return false;

                    runtime.PreviousStateID = runtime.CurrentStateID;
                    runtime.PreviousStateTime = runtime.StateTime;
                    runtime.CurrentStateID = targetStateID;
                    runtime.StateTime = 0.0f;
                    runtime.TransitionTime = 0.0f;
                    runtime.TransitionDuration = FMath::max(0.0f, blendDuration);
                    runtime.bTransitionActive = runtime.TransitionDuration > 1.0e-6f;
                    return true;
                };

                for (const AnimStateAliasTarget& target : alias.Targets)
                {
                    if (tryAliasTarget(target.StateID, target.BlendDuration, &target.Conditions))
                    {
                        didAliasTransition = true;
                        break;
                    }
                }

                if (!didAliasTransition && alias.Targets.IsEmpty())
                {
                    for (const uint64 targetStateID : alias.TargetStateIDs)
                    {
                        if (tryAliasTarget(targetStateID, alias.BlendDuration, nullptr))
                        {
                            didAliasTransition = true;
                            break;
                        }
                    }
                }

                if (!didAliasTransition && alias.Targets.IsEmpty() && alias.TargetStateIDs.IsEmpty())
                    didAliasTransition = tryAliasTarget(alias.ToStateID, alias.BlendDuration, nullptr);

                if (didAliasTransition)
                    break;
            }

            if (!didAliasTransition)
            {
                for (const AnimTransition& transition : stateMachine->Transitions)
                {
                    if (transition.FromStateID == runtime.CurrentStateID && transitionPasses(transition) && findState(transition.ToStateID))
                    {
                        runtime.PreviousStateID = runtime.CurrentStateID;
                        runtime.PreviousStateTime = runtime.StateTime;
                        runtime.CurrentStateID = transition.ToStateID;
                        runtime.StateTime = 0.0f;
                        runtime.TransitionTime = 0.0f;
                        runtime.TransitionDuration = FMath::max(0.0f, transition.BlendDuration);
                        runtime.bTransitionActive = runtime.TransitionDuration > 1.0e-6f;
                        break;
                    }
                }
            }
        }

        runtime.StateTime += dt;
        if (runtime.bTransitionActive)
            runtime.PreviousStateTime += dt;

        const AnimState* currentState = findState(runtime.CurrentStateID);
        if (!currentState)
            return false;

        TArray<uint64> currentVisited;
        TArray<Mat4> currentPose = localBindPose;
        const uint64 currentScopeID = MakeStatePoseGraphRuntimeScopeID(stateMachine->ID, currentState->ID);
        const bool hasCurrent = EvaluateStatePoseGraph(
            skComp, animInstance, assetManager, graph, *currentState, skeleton, localBindPose, runtime.StateTime, dt, bApplyRootMotion, currentPose, currentVisited, currentScopeID, trace);
        if (!hasCurrent)
            return false;

        if (runtime.bTransitionActive)
        {
            const AnimState* previousState = findState(runtime.PreviousStateID);
            if (previousState)
            {
                TArray<uint64> previousVisited;
                TArray<Mat4> previousPose = localBindPose;
                const uint64 previousScopeID = MakeStatePoseGraphRuntimeScopeID(stateMachine->ID, previousState->ID);
                if (EvaluateStatePoseGraph(
                    skComp, animInstance, assetManager, graph, *previousState, skeleton, localBindPose, runtime.PreviousStateTime, dt, bApplyRootMotion, previousPose, previousVisited, previousScopeID, trace))
                {
                    const float alpha = runtime.TransitionDuration > 1.0e-6f
                        ? runtime.TransitionTime / runtime.TransitionDuration
                        : 1.0f;
                    BlendLocalPoses(previousPose, currentPose, alpha, outLocalPose);
                    return true;
                }
            }
        }

        outLocalPose = currentPose;
        return true;
    }
    default:
        break;
    }

    return false;
}

bool EvaluateAnimGraph(
    SkeletalMeshComponent* skComp,
    AnimInstance* animInstance,
    AssetManager& assetManager,
    const AnimGraphAsset& graph,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    const float playbackTime,
    const float dt,
    const bool bApplyRootMotion,
    TArray<Mat4>& outLocalPose,
    AnimationEvaluationTrace* trace)
{
    const AnimGraphNode* output = graph.FindOutputNode();
    if (!output)
        return false;

    TArray<uint64> visited;
    return EvaluateAnimGraphNode(
        skComp,
        animInstance,
        assetManager,
        graph,
        *output,
        skeleton,
        localBindPose,
        playbackTime,
        dt,
        bApplyRootMotion,
        outLocalPose,
        visited,
        true,
        0,
        trace);
}

void LockRootBoneTranslationToBindPose(
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    TArray<Mat4>& inOutLocalPose)
{
    if (!skeleton || localBindPose.Num() != inOutLocalPose.Num())
        return;

    const int32 boneCount = static_cast<int32>(inOutLocalPose.Num());
    auto ToLowerAscii = [](const String& value) -> std::string
    {
        std::string out = value.c_str();
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return out;
    };

    auto IsLocomotionCarrierName = [&](const int32 boneIndex) -> bool
    {
        if (boneIndex < 0 || boneIndex >= skeleton->m_BoneNames.Num())
            return false;

        const std::string name = ToLowerAscii(skeleton->m_BoneNames[boneIndex]);
        return name.find("hips") != std::string::npos ||
               name.find("pelvis") != std::string::npos ||
               name == "root";
    };

    auto lockBoneTranslation = [&](const int32 boneIndex)
    {
        Vector3 bindTranslation(0.0f);
        Vector3 bindScale(1.0f);
        Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localBindPose[boneIndex], bindTranslation, bindRotation, bindScale);

        Vector3 translation(0.0f);
        Vector3 scale(1.0f);
        Quaternion rotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(inOutLocalPose[boneIndex], translation, rotation, scale);

        inOutLocalPose[boneIndex] = AnimationRuntime::ComposeTRS(bindTranslation, rotation, scale);
    };

    for (int32 i = 0; i < boneCount; ++i)
    {
        const int32 parent = skeleton->m_Parent[i];
        if (parent >= 0 && parent < boneCount)
            continue;

        lockBoneTranslation(i);

        int32 locomotionCarrier = -1;
        for (int32 childIndex = 0; childIndex < boneCount; ++childIndex)
        {
            if (skeleton->m_Parent[childIndex] != i)
                continue;

            if (!IsLocomotionCarrierName(childIndex))
                continue;

            locomotionCarrier = childIndex;
            break;
        }

        if (locomotionCarrier >= 0)
            lockBoneTranslation(locomotionCarrier);
    }
}

int32 FindNamedRootBoneIndex(const SkeletonAsset* skeleton)
{
    if (!skeleton)
        return -1;

    for (int32 boneIndex = 0; boneIndex < skeleton->m_BoneNames.Num(); ++boneIndex)
    {
        std::string lowerName = skeleton->m_BoneNames[boneIndex].c_str();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lowerName == "root")
            return boneIndex;
    }

    for (int32 boneIndex = 0; boneIndex < skeleton->m_Parent.Num(); ++boneIndex)
    {
        const int32 parentIndex = skeleton->m_Parent[boneIndex];
        if (parentIndex < 0 || parentIndex >= skeleton->m_Parent.Num())
            return boneIndex;
    }

    return -1;
}

void ApplyRootMotionRootLock(
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    TArray<Mat4>& inOutLocalPose,
    const AnimationAsset* primaryAnimation,
    const ERootMotionRootLock rootLockMode)
{
    if (!skeleton || localBindPose.Num() != inOutLocalPose.Num())
        return;

    const int32 rootBoneIndex = FindNamedRootBoneIndex(skeleton);
    if (rootBoneIndex < 0 || rootBoneIndex >= inOutLocalPose.Num())
        return;

    Vector3 lockedTranslation(0.0f);
    Vector3 lockedScale(1.0f);
    Quaternion lockedRotation(1.0f, 0.0f, 0.0f, 0.0f);

    switch (rootLockMode)
    {
    case ERootMotionRootLock::AnimFirstFrame:
    {
        if (primaryAnimation)
        {
            TArray<Mat4> firstFramePose = localBindPose;
            if (AnimationRuntime::EvaluateLocalPose(
                    skeleton,
                    primaryAnimation,
                    0.0f,
                    localBindPose,
                    firstFramePose,
                    false))
            {
                AnimationRuntime::DecomposeTRS(
                    firstFramePose[rootBoneIndex],
                    lockedTranslation,
                    lockedRotation,
                    lockedScale);
                break;
            }
        }
        [[fallthrough]];
    }
    case ERootMotionRootLock::RefPose:
        AnimationRuntime::DecomposeTRS(
            localBindPose[rootBoneIndex],
            lockedTranslation,
            lockedRotation,
            lockedScale);
        break;

    case ERootMotionRootLock::Zero:
        lockedTranslation = Vector3(0.0f);
        lockedRotation = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
        lockedScale = Vector3(1.0f);
        break;
    }

    Vector3 currentTranslation(0.0f);
    Vector3 currentScale(1.0f);
    Quaternion currentRotation(1.0f, 0.0f, 0.0f, 0.0f);
    AnimationRuntime::DecomposeTRS(
        inOutLocalPose[rootBoneIndex],
        currentTranslation,
        currentRotation,
        currentScale);

    inOutLocalPose[rootBoneIndex] = AnimationRuntime::ComposeTRS(
        lockedTranslation,
        lockedRotation,
        rootLockMode == ERootMotionRootLock::Zero ? lockedScale : currentScale);
}

void StripRootMotionFromVisualPose(
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    TArray<Mat4>& inOutLocalPose,
    const SkeletalMeshComponent* skComp,
    const AnimationEvaluationTrace* trace)
{
    if (!skeleton || localBindPose.Num() != inOutLocalPose.Num())
        return;

    const int32 boneCount = static_cast<int32>(inOutLocalPose.Num());
    const TArray<Mat4> poseBeforeStrip = inOutLocalPose;
    const EVisualRootMotionStripMode requestedStripMode =
        skComp ? skComp->VisualRootMotionStripMode : EVisualRootMotionStripMode::StripTranslation;
    auto ToLowerAscii = [](const String& value) -> std::string
    {
        std::string out = value.c_str();
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return out;
    };

    auto IsLocomotionCarrierName = [&](const int32 boneIndex) -> bool
    {
        if (boneIndex < 0 || boneIndex >= skeleton->m_BoneNames.Num())
            return false;

        const std::string name = ToLowerAscii(skeleton->m_BoneNames[boneIndex]);
        return name.find("hips") != std::string::npos ||
               name.find("pelvis") != std::string::npos ||
               name == "root";
    };

    TArray<Mat4> bindGlobalPose;
    AnimationRuntime::BuildGlobalPose(skeleton, localBindPose, bindGlobalPose);
    const int32 rootBoneIndex = FindNamedRootBoneIndex(skeleton);

    auto ComputeHorizontalVector = [](const Vector3& value) -> Vector3
    {
        Vector3 planar(value.x, value.y, 0.0f);
        const float planarLength = glm::length(planar);
        if (planarLength <= 1.0e-6f)
            return Vector3(0.0f);
        return planar / planarLength;
    };

    auto SelectFacingLocalAxis = [&](const int32 boneIndex) -> Vector3
    {
        if (boneIndex < 0 || boneIndex >= bindGlobalPose.Num())
            return Vector3(1.0f, 0.0f, 0.0f);

        Vector3 bindTranslation(0.0f);
        Vector3 bindScale(1.0f);
        Quaternion bindGlobalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(bindGlobalPose[boneIndex], bindTranslation, bindGlobalRotation, bindScale);

        static const Vector3 candidateAxes[] =
        {
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(-1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, 0.0f, 1.0f),
            Vector3(0.0f, 0.0f, -1.0f)
        };

        Vector3 bestAxis = Vector3(1.0f, 0.0f, 0.0f);
        float bestScore = -FLT_MAX;
        for (const Vector3& axis : candidateAxes)
        {
            const Vector3 worldAxis = bindGlobalRotation * axis;
            const Vector3 planarAxis = ComputeHorizontalVector(worldAxis);
            const float planarStrength = glm::length(Vector2(worldAxis.x, worldAxis.y));
            const float alignment = glm::dot(planarAxis, Vector3(1.0f, 0.0f, 0.0f));
            const float score = planarStrength * 100.0f + alignment * 10.0f - std::fabs(worldAxis.z) * 5.0f;
            if (score > bestScore)
            {
                bestScore = score;
                bestAxis = axis;
            }
        }

        return bestAxis;
    };

    auto ComputeBoneFacingVector = [&](const TArray<Mat4>& globalPose, const int32 boneIndex, const Vector3& localFacingAxis) -> Vector3
    {
        if (boneIndex < 0 || boneIndex >= globalPose.Num())
            return Vector3(1.0f, 0.0f, 0.0f);

        Vector3 translation(0.0f);
        Vector3 scale(1.0f);
        Quaternion globalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(globalPose[boneIndex], translation, globalRotation, scale);
        const Vector3 facing = globalRotation * localFacingAxis;
        const Vector3 planarFacing = ComputeHorizontalVector(facing);
        return glm::length(planarFacing) > 1.0e-6f ? planarFacing : Vector3(1.0f, 0.0f, 0.0f);
    };

    auto ComputeBoneYawDeltaDegrees = [&](const TArray<Mat4>& globalPose, const int32 boneIndex, const Vector3& localFacingAxis) -> float
    {
        const Vector3 bindFacing = ComputeBoneFacingVector(bindGlobalPose, boneIndex, localFacingAxis);
        const Vector3 currentFacing = ComputeBoneFacingVector(globalPose, boneIndex, localFacingAxis);
        return ComputeSignedHorizontalAngleDegrees(bindFacing, currentFacing);
    };

    auto StripBoneYaw = [&](const int32 boneIndex)
    {
        if (boneIndex < 0 || boneIndex >= boneCount)
            return;

        TArray<Mat4> currentGlobalPose;
        AnimationRuntime::BuildGlobalPose(skeleton, inOutLocalPose, currentGlobalPose);

        const Vector3 localFacingAxis = SelectFacingLocalAxis(boneIndex);
        const float yawDegrees = ComputeBoneYawDeltaDegrees(currentGlobalPose, boneIndex, localFacingAxis);

        Vector3 bindTranslation(0.0f);
        Vector3 bindScale(1.0f);
        Quaternion bindLocalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localBindPose[boneIndex], bindTranslation, bindLocalRotation, bindScale);

        Vector3 translation(0.0f);
        Vector3 scale(1.0f);
        Quaternion currentLocalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(inOutLocalPose[boneIndex], translation, currentLocalRotation, scale);

        Vector3 currentGlobalTranslation(0.0f);
        Vector3 currentGlobalScale(1.0f);
        Quaternion currentGlobalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(currentGlobalPose[boneIndex], currentGlobalTranslation, currentGlobalRotation, currentGlobalScale);

        const Quaternion globalCorrection =
            FMath::angleAxis(glm::radians(-yawDegrees), Vector3(0.0f, 0.0f, 1.0f));
        const Quaternion correctedGlobalRotation =
            FMath::normalize(globalCorrection * currentGlobalRotation);

        Quaternion parentGlobalRotation(1.0f, 0.0f, 0.0f, 0.0f);
        const int32 parentIndex = skeleton->m_Parent[boneIndex];
        if (parentIndex >= 0 && parentIndex < currentGlobalPose.Num())
        {
            Vector3 parentTranslation(0.0f);
            Vector3 parentScale(1.0f);
            AnimationRuntime::DecomposeTRS(
                currentGlobalPose[parentIndex],
                parentTranslation,
                parentGlobalRotation,
                parentScale);
        }

        const Quaternion correctedLocalRotation =
            FMath::normalize(FMath::inverse(parentGlobalRotation) * correctedGlobalRotation);

        inOutLocalPose[boneIndex] = AnimationRuntime::ComposeTRS(
            bindTranslation,
            correctedLocalRotation,
            scale);
    };

    TArray<RootMotionBoneDiagnostic> diagnosticsBefore;
    diagnosticsBefore.Reserve(glm::min(boneCount, 10));

    auto CaptureDiagnostic = [&](const TArray<Mat4>& pose, RootMotionBoneDiagnostic& outDiag, const int32 boneIndex)
    {
        outDiag.BoneIndex = boneIndex;

        Vector3 bindTranslation(0.0f);
        Vector3 bindScale(1.0f);
        Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localBindPose[boneIndex], bindTranslation, bindRotation, bindScale);

        Vector3 translation(0.0f);
        Vector3 scale(1.0f);
        Quaternion rotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(pose[boneIndex], translation, rotation, scale);

        outDiag.LocalTranslationDelta = ComputeLocalTranslationDelta(bindTranslation, translation);
    };

    int32 dominantCarrierBone = -1;
    float dominantCarrierScore = 0.0f;
    for (int32 boneIndex = 0; boneIndex < boneCount && boneIndex < 10; ++boneIndex)
    {
        RootMotionBoneDiagnostic diag{};
        CaptureDiagnostic(inOutLocalPose, diag, boneIndex);
        diagnosticsBefore.Add(diag);

        TArray<Mat4> currentGlobalPose;
        AnimationRuntime::BuildGlobalPose(skeleton, inOutLocalPose, currentGlobalPose);
        diag.YawBeforeDegrees = ComputeBoneYawDeltaDegrees(
            currentGlobalPose,
            boneIndex,
            SelectFacingLocalAxis(boneIndex));
        diagnosticsBefore[boneIndex].YawBeforeDegrees = diag.YawBeforeDegrees;

        const bool bNamedCarrier = IsLocomotionCarrierName(boneIndex);
        const int32 depth = GetBoneDepth(skeleton, boneIndex);
        const float score = ComputeBoneCarrierScore(
            diag.YawBeforeDegrees,
            diag.LocalTranslationDelta,
            bNamedCarrier,
            depth);
        if (score > dominantCarrierScore)
        {
            dominantCarrierScore = score;
            dominantCarrierBone = boneIndex;
        }
    }

    const Vector3 rootFacingAxis =
        rootBoneIndex >= 0 ? SelectFacingLocalAxis(rootBoneIndex) : Vector3(1.0f, 0.0f, 0.0f);
    TArray<Mat4> globalPoseBeforeStrip;
    AnimationRuntime::BuildGlobalPose(skeleton, poseBeforeStrip, globalPoseBeforeStrip);
    const float rootYawBeforeDegrees =
        rootBoneIndex >= 0 ? ComputeBoneYawDeltaDegrees(globalPoseBeforeStrip, rootBoneIndex, rootFacingAxis) : 0.0f;
    const bool bRootYawSignificant = std::fabs(rootYawBeforeDegrees) >= 30.0f;

    EVisualRootMotionStripMode effectiveStripMode = requestedStripMode;
    if (requestedStripMode == EVisualRootMotionStripMode::StripTranslation && bRootYawSignificant)
        effectiveStripMode = EVisualRootMotionStripMode::StripTranslationAndYaw;

    const bool bExecuteYawStrip =
        (effectiveStripMode == EVisualRootMotionStripMode::StripYaw ||
         effectiveStripMode == EVisualRootMotionStripMode::StripTranslationAndYaw) &&
        bRootYawSignificant;

    if (bExecuteYawStrip)
    {
        for (int32 i = 0; i < boneCount; ++i)
        {
            const int32 parent = skeleton->m_Parent[i];
            if (parent >= 0 && parent < boneCount)
                continue;

            StripBoneYaw(i);
        }

        if (rootBoneIndex >= 0)
            StripBoneYaw(rootBoneIndex);

        if (dominantCarrierBone >= 0 && dominantCarrierBone != rootBoneIndex)
            StripBoneYaw(dominantCarrierBone);
    }

    static std::unordered_map<const SkeletalMeshComponent*, float> lastLogTimeByComponent;
    const bool bShouldLog = skComp != nullptr;
    if (!bShouldLog)
        return;

    float& lastLogTime = lastLogTimeByComponent[skComp];
    if (trace && std::fabs(trace->PlaybackTime - lastLogTime) < 0.20f)
        return;
    lastLogTime = trace ? trace->PlaybackTime : (lastLogTime + 1.0f);

    TArray<Mat4> globalPoseBefore;
    TArray<Mat4> globalPoseAfter;
    AnimationRuntime::BuildGlobalPose(skeleton, poseBeforeStrip, globalPoseBefore);
    AnimationRuntime::BuildGlobalPose(skeleton, inOutLocalPose, globalPoseAfter);

    String diagnosticLine{};
    const int32 diagnosticCount = glm::min(boneCount, 10);
    for (int32 boneIndex = 0; boneIndex < diagnosticCount; ++boneIndex)
    {
        RootMotionBoneDiagnostic diag{};
        CaptureDiagnostic(inOutLocalPose, diag, boneIndex);
        const Vector3 localFacingAxis = SelectFacingLocalAxis(boneIndex);
        diag.YawAfterDegrees = ComputeBoneYawDeltaDegrees(globalPoseAfter, boneIndex, localFacingAxis);

        const RootMotionBoneDiagnostic& beforeDiag = diagnosticsBefore[boneIndex];
        diagnosticLine += "[";
        diagnosticLine += std::to_string(boneIndex).c_str();
        diagnosticLine += ":";
        diagnosticLine += skeleton->m_BoneNames[boneIndex];
        diagnosticLine += " yaw ";
        diagnosticLine += std::to_string(beforeDiag.YawBeforeDegrees).c_str();
        diagnosticLine += " -> ";
        diagnosticLine += std::to_string(diag.YawAfterDegrees).c_str();
        diagnosticLine += " t ";
        diagnosticLine += std::to_string(beforeDiag.LocalTranslationDelta).c_str();
        diagnosticLine += "] ";
    }

    Vector3 actorForward(1.0f, 0.0f, 0.0f);
    if (skComp->GetOwner())
        actorForward = skComp->GetOwner()->GetActorForwardVector();

    auto FindBoneIndexByName = [&](const char* boneName) -> int32
    {
        for (int32 boneIndex = 0; boneIndex < skeleton->m_BoneNames.Num(); ++boneIndex)
        {
            if (skeleton->m_BoneNames[boneIndex] == boneName)
                return boneIndex;
        }
        return -1;
    };

    const Vector3 dominantFacingAxis =
        dominantCarrierBone >= 0 ? SelectFacingLocalAxis(dominantCarrierBone) : Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 carrierForwardBefore =
        dominantCarrierBone >= 0 ? ComputeBoneFacingVector(globalPoseBefore, dominantCarrierBone, dominantFacingAxis)
                                 : Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 carrierForwardAfter =
        dominantCarrierBone >= 0 ? ComputeBoneFacingVector(globalPoseAfter, dominantCarrierBone, dominantFacingAxis)
                                 : Vector3(1.0f, 0.0f, 0.0f);
    const int32 debugRootBoneIndex = FindBoneIndexByName("root");
    const int32 hipsBoneIndex = FindBoneIndexByName("hips");
    const int32 pelvisBoneIndex = FindBoneIndexByName("pelvis");

    RB_LOG(AnimationModuleLog,
           info,
           "RootMotionStrip path={} clips={} rootMotionEnabled={} playbackTime={} rootLock={} stripMode={} effectiveStripMode={} yawStrip={} rootYawSignificant={} root={} hips={} pelvis={} dominantBone={} dominantName={} actorForward=({}, {}, {}) meshForwardBefore=({}, {}, {}) meshForwardAfter=({}, {}, {}) bones={}",
           trace ? trace->Path : "Unknown",
           trace ? trace->ClipSummary.c_str() : "",
           skComp->bEnableRootMotion ? 1 : 0,
           trace ? trace->PlaybackTime : 0.0f,
           skComp ? ToString(skComp->RootMotionRootLock) : "RefPose",
           ToString(requestedStripMode),
           ToString(effectiveStripMode),
           bExecuteYawStrip ? "executed" : "skipped",
           bRootYawSignificant ? 1 : 0,
           debugRootBoneIndex,
           hipsBoneIndex,
           pelvisBoneIndex,
           dominantCarrierBone,
           (dominantCarrierBone >= 0 && dominantCarrierBone < skeleton->m_BoneNames.Num()) ? skeleton->m_BoneNames[dominantCarrierBone].c_str() : "",
           actorForward.x,
           actorForward.y,
           actorForward.z,
           carrierForwardBefore.x,
           carrierForwardBefore.y,
           carrierForwardBefore.z,
           carrierForwardAfter.x,
           carrierForwardAfter.y,
           carrierForwardAfter.z,
           diagnosticLine.c_str());
}

AnimationLocomotionState BuildLocomotionStateForComponent(const SkeletalMeshComponent* skComp)
{
    if (!skComp)
        return {};

    const Actor* owner = skComp->GetOwner();
    if (!owner)
        return skComp->LocomotionState;

    const Character* character = dynamic_cast<const Character*>(owner);
    if (!character)
        return skComp->LocomotionState;

    const CharacterMovementComponent* movement = character->GetCharacterMovementComponent();
    if (!movement)
        return skComp->LocomotionState;
    
    AnimationLocomotionState locomotionState = movement->BuildAnimationLocomotionState();
    locomotionState.bIsMoving = locomotionState.HorizontalSpeed > 0.01f;
    locomotionState.Action = locomotionState.bJumpStarted
        ? ELocomotionAction::Jumping
        : (locomotionState.bLanded ? ELocomotionAction::Landing : ELocomotionAction::None);
    locomotionState.Gait = locomotionState.HorizontalSpeed > 4.75f
        ? EGait::Sprint
        : (locomotionState.HorizontalSpeed > 2.5f ? EGait::Run : EGait::Walk);
    locomotionState.Stance = EStance::Standing;
    locomotionState.RotationMode = ERotationMode::VelocityDirection;
    locomotionState.SemanticLocomotionState = locomotionState.bIsGrounded
        ? ELocomotionState::Grounded
        : ELocomotionState::InAir;

    const LocomotionCharacter* locomotionCharacter = dynamic_cast<const LocomotionCharacter*>(character);
    if (!locomotionCharacter || !locomotionCharacter->GetLocomotionComponent())
        return locomotionState;

    const FLocomotionState& semanticState = locomotionCharacter->GetLocomotionState();
    locomotionState.HorizontalSpeed = semanticState.Speed;
    locomotionState.VerticalSpeed = semanticState.VerticalSpeed;
    locomotionState.bHasMovementInput = semanticState.bHasMovementInput;
    locomotionState.bIsMoving = semanticState.bIsMoving;
    locomotionState.bJumpStarted = semanticState.bJustJumped;
    locomotionState.bLanded = semanticState.bJustLanded;
    locomotionState.Gait = semanticState.Gait;
    locomotionState.Stance = semanticState.Stance;
    locomotionState.RotationMode = semanticState.RotationMode;
    locomotionState.Action = semanticState.Action;
    locomotionState.SemanticLocomotionState = semanticState.LocomotionState;
    locomotionState.MoveDirectionLocalDegrees = semanticState.MoveDirectionLocal;
    locomotionState.VelocityYawDelta = semanticState.VelocityYawDelta;
    locomotionState.AimYawDelta = semanticState.AimYawDelta;
    locomotionState.MovementMode = semanticState.LocomotionState == ELocomotionState::Grounded
        ? AnimationMovementMode::Walking
        : AnimationMovementMode::Falling;
    locomotionState.bIsGrounded = semanticState.LocomotionState == ELocomotionState::Grounded;

    const float moveDirectionRadians = glm::radians(semanticState.MoveDirectionLocal);
    locomotionState.LocalMoveDirection = Vector2(std::sin(moveDirectionRadians), std::cos(moveDirectionRadians));

    return locomotionState;
}
} // namespace

AnimationModule::AnimationModule()
{
    m_TickType = TickType::PostSimulation;
}

AnimationModule::~AnimationModule()
{
}

void AnimationModule::OnEvent(const Event& e)
{
    IModule::OnEvent(e);
}

void AnimationModule::Init()
{
}

void AnimationModule::UpdateAnimations(float dt)
{
    if (!m_Scene || !m_AssetManager)
        return;

    AssetManager& assetManager = *m_AssetManager;
    auto& reg = m_Scene->GetRegistry();
    auto skelView = reg.view<SkeletalMeshComponent*>();

    for (auto e : skelView)
    {
        auto* skComp = skelView.get<SkeletalMeshComponent*>(e);
        if (!skComp)
            continue;

        if (!skComp->bIsVisible || !skComp->IsValid())
            continue;

        SkeletalMeshAsset* skAsset =
            dynamic_cast<SkeletalMeshAsset*>(assetManager.Load(skComp->Mesh.GetHandle()));
        if (!skAsset)
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        SkeletonAsset* skeleton =
            dynamic_cast<SkeletonAsset*>(assetManager.Load(skAsset->m_Skeleton.GetHandle()));
        if (!skeleton)
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        TArray<Mat4> localBindPose;
        TArray<Mat4> globalBindPose;
        if (!AnimationRuntime::BuildBindPoses(skeleton, localBindPose, globalBindPose))
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        ValidateBindPoseMatricesOnce(skeleton, globalBindPose);

        TArray<Mat4> localPose = localBindPose;
        TArray<Mat4> globalPose = globalBindPose;

        const bool bWantsOverrideAnimation =
            skComp->bOverrideAnimationActive &&
            (uint64)skComp->OverrideAnimation.GetHandle() != 0;

        bool bPoseEvaluated = false;
        AnimationEvaluationTrace evaluationTrace{};
        evaluationTrace.PlaybackTime = skComp->PlaybackTime;
        AnimGraphAsset* animGraph = nullptr;
        if (IsValidAssetHandle(skComp->AnimGraph.GetHandle()))
            animGraph = dynamic_cast<AnimGraphAsset*>(assetManager.Load(skComp->AnimGraph.GetHandle()));

        const Rebel::Core::Reflection::TypeInfo* requestedAnimInstanceType = nullptr;
        if (animGraph && animGraph->m_AnimInstanceClass.Get())
            requestedAnimInstanceType = animGraph->m_AnimInstanceClass.Get();
        else if (skComp->AnimInstanceClass.Get())
            requestedAnimInstanceType = skComp->AnimInstanceClass.Get();

        AnimInstance* animInstance = skComp->EnsureAnimInstance(requestedAnimInstanceType);
        if (animInstance && !bWantsOverrideAnimation)
        {
            animInstance->SetRootMotionEnabled(skComp->bEnableRootMotion);
            skComp->LocomotionState = BuildLocomotionStateForComponent(skComp);
            if (skComp->bPlayAnimation)
                animInstance->Update(dt * skComp->PlaybackSpeed, skComp->LocomotionState);

            AnimationEvaluationContext context{};
            context.AssetManager = &assetManager;
            context.Skeleton = skeleton;
            context.LocalBindPose = &localBindPose;

            if (animGraph)
            {
                if (skComp->bPlayAnimation)
                    skComp->PlaybackTime += dt * skComp->PlaybackSpeed;

                evaluationTrace.Path = "AnimGraph";
                evaluationTrace.PlaybackTime = skComp->PlaybackTime;
                bPoseEvaluated = EvaluateAnimGraph(
                    skComp,
                    animInstance,
                    assetManager,
                    *animGraph,
                    skeleton,
                    localBindPose,
                    skComp->PlaybackTime,
                    dt * skComp->PlaybackSpeed,
                    skComp->bEnableRootMotion,
                    localPose,
                    &evaluationTrace);
            }
            else
            {
                evaluationTrace.Path = "AnimInstance";
                bPoseEvaluated = animInstance->Evaluate(context, localPose);
                skComp->PlaybackTime = animInstance->GetDebugPlaybackTime();
                evaluationTrace.PlaybackTime = skComp->PlaybackTime;
            }
        }

        if (!bPoseEvaluated)
        {
            AnimationAsset* overrideAnimation = nullptr;
            if (bWantsOverrideAnimation)
            {
                overrideAnimation = dynamic_cast<AnimationAsset*>(
                    assetManager.Load(skComp->OverrideAnimation.GetHandle()));

                if (overrideAnimation &&
                    (uint64)overrideAnimation->m_SkeletonID != 0 &&
                    (uint64)overrideAnimation->m_SkeletonID != (uint64)skeleton->ID)
                {
                    overrideAnimation = nullptr;
                }

                if (!overrideAnimation)
                    skComp->StopAnimation();
            }

            if (overrideAnimation)
            {
                if (skComp->bPlayAnimation)
                    skComp->OverridePlaybackTime += dt * skComp->OverridePlaybackSpeed;

                skComp->OverridePlaybackTime = AnimationRuntime::NormalizePlaybackTime(
                    skComp->OverridePlaybackTime,
                    overrideAnimation->m_DurationSeconds,
                    skComp->bOverrideAnimationLooping);

                TArray<Mat4> overridePose = localBindPose;
                evaluationTrace.Path = "Override";
                evaluationTrace.PlaybackTime = skComp->OverridePlaybackTime;
                evaluationTrace.ClipSummary = overrideAnimation->m_ClipName;
                evaluationTrace.PrimaryAnimation = overrideAnimation;
                bPoseEvaluated = AnimationRuntime::EvaluateLocalPose(
                    skeleton,
                    overrideAnimation,
                    skComp->OverridePlaybackTime,
                    localBindPose,
                    overridePose,
                    skComp->bEnableRootMotion);

                if (bPoseEvaluated && skComp->bOverrideBlendActive)
                {
                    if (skComp->bPlayAnimation)
                    {
                        skComp->OverrideBlendElapsed += dt;
                        if (skComp->OverrideBlendElapsed > skComp->OverrideBlendDuration)
                            skComp->OverrideBlendElapsed = skComp->OverrideBlendDuration;
                    }

                    const float blendAlpha = skComp->OverrideBlendDuration > 1.0e-6f
                        ? FMath::clamp(skComp->OverrideBlendElapsed / skComp->OverrideBlendDuration, 0.0f, 1.0f)
                        : 1.0f;

                    BlendLocalPoses(
                        skComp->OverrideBlendSourcePose,
                        overridePose,
                        blendAlpha,
                        localPose);

                    if (blendAlpha >= 1.0f - 1.0e-6f)
                    {
                        skComp->bOverrideBlendActive = false;
                        skComp->OverrideBlendDuration = 0.0f;
                        skComp->OverrideBlendElapsed = 0.0f;
                        skComp->OverrideBlendSourcePose.Clear();
                    }
                }
                else if (bPoseEvaluated)
                {
                    localPose = std::move(overridePose);
                }

                if (bPoseEvaluated && skComp->bOverrideLockRootBoneTranslation)
                    LockRootBoneTranslationToBindPose(skeleton, localBindPose, localPose);

                skComp->PlaybackTime = skComp->OverridePlaybackTime;

                if (!skComp->bOverrideAnimationLooping &&
                    overrideAnimation->m_DurationSeconds > 0.0f &&
                    skComp->OverridePlaybackTime >= overrideAnimation->m_DurationSeconds - 1.0e-4f)
                {
                    const float blendOutDuration = glm::max(0.0f, skComp->OverrideBlendOutDuration);
                    const bool bCanBlendOut = blendOutDuration > 1.0e-6f && !localPose.IsEmpty();

                    skComp->bOverrideAnimationActive = false;
                    skComp->OverrideAnimation = {};
                    skComp->OverridePlaybackTime = 0.0f;
                    skComp->bOverrideBlendActive = false;
                    skComp->OverrideBlendDuration = 0.0f;
                    skComp->OverrideBlendElapsed = 0.0f;
                    skComp->OverrideBlendSourcePose.Clear();
                    skComp->bOverrideLockRootBoneTranslation = false;

                    skComp->bOverrideBlendOutActive = bCanBlendOut;
                    skComp->OverrideBlendOutElapsed = 0.0f;
                    if (bCanBlendOut)
                        skComp->OverrideBlendOutSourcePose = localPose;
                    else
                        skComp->OverrideBlendOutSourcePose.Clear();
                }
            }

            if (!bPoseEvaluated)
            {
                AnimationAsset* animation = nullptr;
                if ((uint64)skComp->Animation.GetHandle() != 0)
                {
                    animation = dynamic_cast<AnimationAsset*>(assetManager.Load(skComp->Animation.GetHandle()));

                    if (animation && (uint64)animation->m_SkeletonID != 0 &&
                        (uint64)animation->m_SkeletonID != (uint64)skeleton->ID)
                    {
                        animation = nullptr;
                    }
                }

                if (animation)
                {
                    if (skComp->bPlayAnimation)
                        skComp->PlaybackTime += dt * skComp->PlaybackSpeed;

                    skComp->PlaybackTime = AnimationRuntime::NormalizePlaybackTime(
                        skComp->PlaybackTime,
                        animation->m_DurationSeconds,
                        skComp->bLoopAnimation);

                    evaluationTrace.Path = "Direct";
                    evaluationTrace.PlaybackTime = skComp->PlaybackTime;
                    evaluationTrace.ClipSummary = animation->m_ClipName;
                    evaluationTrace.PrimaryAnimation = animation;
                    bPoseEvaluated = AnimationRuntime::EvaluateLocalPose(
                        skeleton,
                        animation,
                        skComp->PlaybackTime,
                        localBindPose,
                        localPose,
                        skComp->bEnableRootMotion);

                }
                else
                {
                    skComp->PlaybackTime = 0.0f;
                }
            }
        }

        if (!bPoseEvaluated)
            localPose = localBindPose;

        if (bPoseEvaluated && !skComp->bEnableRootMotion)
        {
            ApplyRootMotionRootLock(
                skeleton,
                localBindPose,
                localPose,
                evaluationTrace.PrimaryAnimation,
                skComp->RootMotionRootLock);
            StripRootMotionFromVisualPose(skeleton, localBindPose, localPose, skComp, &evaluationTrace);
        }

        if (skComp->bOverrideBlendOutActive)
        {
            const bool bValidBlendOutSource =
                !skComp->OverrideBlendOutSourcePose.IsEmpty() &&
                skComp->OverrideBlendOutSourcePose.Num() == localPose.Num();
            if (!bValidBlendOutSource)
            {
                skComp->bOverrideBlendOutActive = false;
                skComp->OverrideBlendOutElapsed = 0.0f;
                skComp->OverrideBlendOutSourcePose.Clear();
            }
            else
            {
                if (skComp->bPlayAnimation)
                {
                    skComp->OverrideBlendOutElapsed += dt;
                    if (skComp->OverrideBlendOutElapsed > skComp->OverrideBlendOutDuration)
                        skComp->OverrideBlendOutElapsed = skComp->OverrideBlendOutDuration;
                }

                const float blendAlpha = skComp->OverrideBlendOutDuration > 1.0e-6f
                    ? FMath::clamp(
                        skComp->OverrideBlendOutElapsed / skComp->OverrideBlendOutDuration,
                        0.0f,
                        1.0f)
                    : 1.0f;

                TArray<Mat4> blendedPose;
                BlendLocalPoses(skComp->OverrideBlendOutSourcePose, localPose, blendAlpha, blendedPose);
                localPose = std::move(blendedPose);

                if (blendAlpha >= 1.0f - 1.0e-6f)
                {
                    skComp->bOverrideBlendOutActive = false;
                    skComp->OverrideBlendOutElapsed = 0.0f;
                    skComp->OverrideBlendOutSourcePose.Clear();
                }
            }
        }

        AnimationRuntime::BuildGlobalPose(skeleton, localPose, globalPose);

        TArray<Mat4> skinPalette;
        if (!AnimationRuntime::BuildSkinPalette(skeleton, globalPose, skinPalette))
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        skComp->LocalPose = std::move(localPose);
        skComp->GlobalPose = std::move(globalPose);
        skComp->FinalPalette = std::move(skinPalette);

        UpdateRuntimeBoneTransforms(skComp, skeleton, skComp->LocalPose, skComp->GlobalPose);
    }
}

void AnimationModule::Tick(float deltaTime)
{
    UpdateAnimations(deltaTime);
}

void AnimationModule::Shutdown()
{
}




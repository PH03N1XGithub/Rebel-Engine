#include "Editor/Panels/AnimationDebuggerPanel.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Animation/SkeletonAsset.h"

#include "imgui.h"

AnimationDebuggerPanel::AnimationDebuggerPanel(EditorSelection& selection)
    : m_Selection(selection)
{
}

static float NormalizeDebugAnimationTime(float timeSeconds, float durationSeconds, bool bLooping)
{
    return AnimationRuntime::NormalizePlaybackTime(timeSeconds, durationSeconds, bLooping);
}

template<typename TKey>
static int32 FindDebugKeySpanStart(const TArray<TKey>& keys, float sampleTime)
{
    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 1)
        return 0;

    if (sampleTime <= keys[0].TimeSeconds)
        return 0;

    auto begin = keys.begin();
    auto end = keys.end();
    auto upper = std::lower_bound(
        begin + 1,
        end,
        sampleTime,
        [](const TKey& key, float time)
        {
            return key.TimeSeconds < time;
        });

    if (upper == end)
        return keyCount - 1;

    const int32 upperIndex = static_cast<int32>(upper - begin);
    return upperIndex - 1;
}

static Vector3 SampleDebugAnimationVectorKeys(
    const TArray<AnimationVecKey>& keys,
    float sampleTime,
    int32* outFromKeyIndex = nullptr,
    int32* outToKeyIndex = nullptr,
    float* outAlpha = nullptr)
{
    if (outFromKeyIndex) *outFromKeyIndex = -1;
    if (outToKeyIndex) *outToKeyIndex = -1;
    if (outAlpha) *outAlpha = 0.0f;

    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 0)
        return Vector3(0.0f);

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
    {
        if (outFromKeyIndex) *outFromKeyIndex = 0;
        if (outToKeyIndex) *outToKeyIndex = 0;
        return keys[0].Value;
    }

    const int32 fromIndex = FindDebugKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
    {
        if (outFromKeyIndex) *outFromKeyIndex = keyCount - 1;
        if (outToKeyIndex) *outToKeyIndex = keyCount - 1;
        return keys[keyCount - 1].Value;
    }

    const AnimationVecKey& a = keys[fromIndex];
    const AnimationVecKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
    {
        if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
        if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
        if (outAlpha) *outAlpha = 1.0f;
        return b.Value;
    }

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
    if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
    if (outAlpha) *outAlpha = alpha;
    return FMath::mix(a.Value, b.Value, alpha);
}

static Quaternion SampleDebugAnimationRotationKeys(
    const TArray<AnimationQuatKey>& keys,
    float sampleTime,
    int32* outFromKeyIndex = nullptr,
    int32* outToKeyIndex = nullptr,
    float* outAlpha = nullptr)
{
    if (outFromKeyIndex) *outFromKeyIndex = -1;
    if (outToKeyIndex) *outToKeyIndex = -1;
    if (outAlpha) *outAlpha = 0.0f;

    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 0)
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
    {
        if (outFromKeyIndex) *outFromKeyIndex = 0;
        if (outToKeyIndex) *outToKeyIndex = 0;
        return FMath::normalize(keys[0].Value);
    }

    const int32 fromIndex = FindDebugKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
    {
        if (outFromKeyIndex) *outFromKeyIndex = keyCount - 1;
        if (outToKeyIndex) *outToKeyIndex = keyCount - 1;
        return FMath::normalize(keys[keyCount - 1].Value);
    }

    const AnimationQuatKey& a = keys[fromIndex];
    const AnimationQuatKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
    {
        if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
        if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
        if (outAlpha) *outAlpha = 1.0f;
        return FMath::normalize(b.Value);
    }

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    Quaternion qb = b.Value;
    if (FMath::dot(a.Value, qb) < 0.0f)
        qb = -qb;

    if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
    if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
    if (outAlpha) *outAlpha = alpha;

    return FMath::normalize(FMath::slerp(a.Value, qb, alpha));
}

static float QuaternionAngularDifferenceDegrees(const Quaternion& a, const Quaternion& b)
{
    const Quaternion na = FMath::normalize(a);
    const Quaternion nb = FMath::normalize(b);
    const float dotValue = FMath::clamp(std::fabs(FMath::dot(na, nb)), 0.0f, 1.0f);
    const float angleRad = 2.0f * std::acos(dotValue);
    return FMath::degrees(angleRad);
}

static const char* BoolLabel(const bool value)
{
    return value ? "true" : "false";
}

static const char* ToDebugString(const ELocomotionState state)
{
    switch (state)
    {
    case ELocomotionState::Grounded: return "Grounded";
    case ELocomotionState::InAir: return "InAir";
    default: return "Unknown";
    }
}

static const char* ToDebugString(const ELocomotionAction action)
{
    switch (action)
    {
    case ELocomotionAction::None: return "None";
    case ELocomotionAction::Starting: return "Starting";
    case ELocomotionAction::Stopping: return "Stopping";
    case ELocomotionAction::Pivoting: return "Pivoting";
    case ELocomotionAction::Jumping: return "Jumping";
    case ELocomotionAction::Landing: return "Landing";
    default: return "Unknown";
    }
}

static const AnimState* FindDebugAnimState(const AnimStateMachine& stateMachine, const uint64 stateID)
{
    for (const AnimState& state : stateMachine.States)
    {
        if (state.ID == stateID)
            return &state;
    }

    return nullptr;
}

void AnimationDebuggerPanel::Draw()
{
    Actor* selected = m_Selection.SelectedActor;
    if (!selected || !selected->IsValid() || !selected->HasComponent<SkeletalMeshComponent>())
    {
        ImGui::TextDisabled("Select an actor with SkeletalMeshComponent.");
        return;
    }

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        ImGui::TextDisabled("Asset manager is unavailable.");
        return;
    }

    SkeletalMeshComponent& skComp = selected->GetComponent<SkeletalMeshComponent>();
    auto& manager = assetModule->GetManager();
    const auto& registry = assetModule->GetRegistry().GetAll();

    SkeletalMeshAsset* skAsset = nullptr;
    SkeletonAsset* skeleton = nullptr;
    if ((uint64)skComp.Mesh.GetHandle() != 0)
    {
        skAsset = dynamic_cast<SkeletalMeshAsset*>(manager.Load(skComp.Mesh.GetHandle()));
        if (skAsset && (uint64)skAsset->m_Skeleton.GetHandle() != 0)
            skeleton = dynamic_cast<SkeletonAsset*>(manager.Load(skAsset->m_Skeleton.GetHandle()));
    }

    AnimGraphAsset* animGraph = nullptr;
    const char* animGraphLabel = "None";
    if ((uint64)skComp.AnimGraph.GetHandle() != 0)
    {
        animGraph = dynamic_cast<AnimGraphAsset*>(manager.Load(skComp.AnimGraph.GetHandle()));
        if (const AssetMeta* meta = registry.Find(skComp.AnimGraph.GetHandle()))
            animGraphLabel = meta->Path.c_str();
    }

    const char* animationLabel = "None";
    if ((uint64)skComp.Animation.GetHandle() != 0)
    {
        if (const AssetMeta* meta = registry.Find(skComp.Animation.GetHandle()))
            animationLabel = meta->Path.c_str();
    }

    if (ImGui::CollapsingHeader("Runtime State", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Actor: %s", selected->GetName().c_str());
        ImGui::Text("Anim Graph: %s", animGraphLabel);

        if (AnimInstance* animInstance = skComp.GetAnimInstance())
        {
            ImGui::Text("Anim Instance State: %s", animInstance->GetDebugStateName());
            ImGui::Text("State Playback Time: %.3f", animInstance->GetDebugPlaybackTime());
        }
        else
        {
            ImGui::TextDisabled("Anim instance not initialized.");
        }

        const AnimationLocomotionState& locomotionState = skComp.LocomotionState;
        ImGui::SeparatorText("Locomotion");
        ImGui::Text("Semantic State: %s", ToDebugString(locomotionState.SemanticLocomotionState));
        ImGui::Text("Action: %s", ToDebugString(locomotionState.Action));
        ImGui::Text("Horizontal Speed: %.3f", locomotionState.HorizontalSpeed);
        ImGui::Text("Vertical Speed: %.3f", locomotionState.VerticalSpeed);
        ImGui::Text("Ground Distance: %.3f", locomotionState.GroundDistance);
        ImGui::Text("Time In Air: %.3f", locomotionState.TimeInAir);
        ImGui::Text("Is Grounded: %s", BoolLabel(locomotionState.bIsGrounded));
        ImGui::Text("Has Movement Input: %s", BoolLabel(locomotionState.bHasMovementInput));
        ImGui::Text("Is Moving: %s", BoolLabel(locomotionState.bIsMoving));
        ImGui::Text("Jump Started: %s", BoolLabel(locomotionState.bJumpStarted));
        ImGui::Text("Landed: %s", BoolLabel(locomotionState.bLanded));

        ImGui::SeparatorText("State Machines");
        if (skComp.StateMachineRuntimes.IsEmpty())
        {
            ImGui::TextDisabled("No state machine runtime data.");
        }
        else
        {
            for (const AnimStateMachineRuntime& runtime : skComp.StateMachineRuntimes)
            {
                const AnimStateMachine* stateMachine = animGraph
                    ? animGraph->FindStateMachine(runtime.StateMachineID)
                    : nullptr;
                const char* stateMachineName = stateMachine ? stateMachine->Name.c_str() : "Unknown";
                const AnimState* currentState =
                    stateMachine ? FindDebugAnimState(*stateMachine, runtime.CurrentStateID) : nullptr;
                const AnimState* previousState =
                    stateMachine ? FindDebugAnimState(*stateMachine, runtime.PreviousStateID) : nullptr;
                const char* currentStateName = currentState ? currentState->Name.c_str() : "None";
                const char* previousStateName = previousState ? previousState->Name.c_str() : "None";

                ImGui::PushID(static_cast<int>(runtime.StateMachineID));
                ImGui::Separator();
                ImGui::Text("State Machine: %s", stateMachineName);
                ImGui::Text("Current State: %s", currentStateName);
                ImGui::Text("State Time: %.3f", runtime.StateTime);

                if (runtime.bTransitionActive)
                {
                    const float transitionAlpha = runtime.TransitionDuration > 1e-6f
                        ? FMath::clamp(runtime.TransitionTime / runtime.TransitionDuration, 0.0f, 1.0f)
                        : 1.0f;
                    ImGui::Text("Transition: %s -> %s", previousStateName, currentStateName);
                    ImGui::Text("Transition Time: %.3f / %.3f (alpha %.2f)",
                        runtime.TransitionTime,
                        runtime.TransitionDuration,
                        transitionAlpha);
                    ImGui::Text("Previous State Time: %.3f", runtime.PreviousStateTime);
                }
                else
                {
                    ImGui::TextDisabled("Transition: inactive");
                }

                ImGui::PopID();
            }
        }

        if (!skComp.BlendNodeRuntimes.IsEmpty())
        {
            ImGui::SeparatorText("Blend Nodes");
            for (const AnimBlendNodeRuntime& runtime : skComp.BlendNodeRuntimes)
            {
                const char* nodeName = "Unknown";
                if (animGraph)
                {
                    if (const AnimGraphNode* node = animGraph->FindNode(runtime.NodeID))
                        nodeName = node->Name.c_str();
                }

                ImGui::PushID(static_cast<int>(runtime.NodeID));
                ImGui::Text("%s  alpha %.2f  scope %llu  node %llu",
                    nodeName,
                    runtime.CurrentAlpha,
                    static_cast<unsigned long long>(runtime.ScopeID),
                    static_cast<unsigned long long>(runtime.NodeID));
                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

    if (ImGui::BeginCombo("Animation", animationLabel))
    {
        if (ImGui::Selectable("None", (uint64)skComp.Animation.GetHandle() == 0))
        {
            skComp.Animation.SetHandle(0);
            skComp.PlaybackTime = 0.0f;
        }

        for (const auto& pair : registry)
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != AnimationAsset::StaticType())
                continue;

            bool bCompatible = true;
            if (skeleton)
            {
                AnimationAsset* candidate = dynamic_cast<AnimationAsset*>(manager.Load(meta.ID));
                if (candidate && (uint64)candidate->m_SkeletonID != 0 &&
                    (uint64)candidate->m_SkeletonID != (uint64)skeleton->ID)
                {
                    bCompatible = false;
                }
            }

            if (!bCompatible)
                continue;

            const bool bSelected = (skComp.Animation.GetHandle() == meta.ID);
            if (ImGui::Selectable(meta.Path.c_str(), bSelected))
            {
                skComp.Animation.SetHandle(meta.ID);
                skComp.PlaybackTime = 0.0f;
            }

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::Checkbox("Play", &skComp.bPlayAnimation);
    ImGui::Checkbox("Loop", &skComp.bLoopAnimation);
    ImGui::DragFloat("Speed", &skComp.PlaybackSpeed, 0.01f, -4.0f, 4.0f);
    ImGui::DragFloat("Time", &skComp.PlaybackTime, 0.01f, 0.0f, 600.0f);

    AnimationAsset* animation = nullptr;
    if ((uint64)skComp.Animation.GetHandle() != 0)
        animation = dynamic_cast<AnimationAsset*>(manager.Load(skComp.Animation.GetHandle()));
    const float sampleTime = animation
        ? NormalizeDebugAnimationTime(skComp.PlaybackTime, animation->m_DurationSeconds, skComp.bLoopAnimation)
        : 0.0f;

    if (animation)
    {
        int32 missingTrackCount = 0;
        bool selectedBoneHasTrack = false;
        if (skeleton)
        {
            for (int32 boneIndex = 0; boneIndex < skeleton->m_BoneNames.Num(); ++boneIndex)
            {
                const bool hasTrack = (animation->FindTrackForBone(boneIndex) != nullptr);
                if (!hasTrack)
                    ++missingTrackCount;

                if (boneIndex == m_SelectedBone)
                    selectedBoneHasTrack = hasTrack;
            }
        }

        ImGui::Text("Clip: %s", animation->m_ClipName.c_str());
        ImGui::Text("Duration: %.3f", animation->m_DurationSeconds);
        ImGui::Text("Tracks: %d", animation->m_Tracks.Num());
        if (skeleton)
        {
            ImGui::Text("Missing Bone Tracks: %d / %d", missingTrackCount, skeleton->m_BoneNames.Num());
            ImGui::Text("Selected Bone Has Track: %s", selectedBoneHasTrack ? "yes" : "no");
        }
        if (animation->m_RootDriver.bEnabled)
        {
            ImGui::Text("Root Driver: %s (T:%d R:%d S:%d)",
                animation->m_RootDriver.NodeName.c_str(),
                animation->m_RootDriver.bAffectsTranslation ? 1 : 0,
                animation->m_RootDriver.bAffectsRotation ? 1 : 0,
                animation->m_RootDriver.bAffectsScale ? 1 : 0);
        }
        else
        {
            ImGui::TextDisabled("Root Driver: none");
        }
    }
    else
    {
        ImGui::TextDisabled("No animation selected.");
    }

    if (!skeleton || skeleton->m_BoneNames.IsEmpty())
    {
        ImGui::TextDisabled("No skeleton data available.");
        return;
    }

    if (m_SelectedBone < 0 || m_SelectedBone >= skeleton->m_BoneNames.Num())
        m_SelectedBone = 0;

    const char* boneLabel = skeleton->m_BoneNames[m_SelectedBone].c_str();
    if (ImGui::BeginCombo("Bone", boneLabel))
    {
        for (int32 i = 0; i < skeleton->m_BoneNames.Num(); ++i)
        {
            const bool bSelected = (i == m_SelectedBone);
            if (ImGui::Selectable(skeleton->m_BoneNames[i].c_str(), bSelected))
                m_SelectedBone = i;

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const int32 boneCount = static_cast<int32>(skeleton->m_BoneNames.Num());
    const bool hasRuntimePoseData =
        skComp.RuntimeBoneLocalTranslations.Num() == boneCount &&
        skComp.RuntimeBoneGlobalTranslations.Num() == boneCount &&
        skComp.RuntimeBoneLocalScales.Num() == boneCount &&
        skComp.RuntimeBoneGlobalScales.Num() == boneCount &&
        skComp.RuntimeBoneLocalRotations.Num() == boneCount &&
        skComp.RuntimeBoneGlobalRotations.Num() == boneCount;

    Vector3 runtimeLocalT(0.0f);
    Vector3 runtimeGlobalT(0.0f);
    Vector3 runtimeLocalS(1.0f);
    Vector3 runtimeGlobalS(1.0f);
    Quaternion runtimeLocalR(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion runtimeGlobalR(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 runtimeLocalEuler(0.0f);
    Vector3 runtimeGlobalEuler(0.0f);
    bool hasAssetPoseData = false;
    Vector3 assetPoseLocalT(0.0f);
    Vector3 assetPoseGlobalT(0.0f);
    Vector3 assetPoseLocalS(1.0f);
    Vector3 assetPoseGlobalS(1.0f);
    Quaternion assetPoseLocalR(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion assetPoseGlobalR(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 assetPoseLocalEuler(0.0f);
    Vector3 assetPoseGlobalEuler(0.0f);

    if (animation)
    {
        TArray<Mat4> localBindPose;
        TArray<Mat4> globalBindPose;
        if (AnimationRuntime::BuildBindPoses(skeleton, localBindPose, globalBindPose))
        {
            TArray<Mat4> localPose = localBindPose;
            TArray<Mat4> globalPose = globalBindPose;
            AnimationRuntime::EvaluateLocalPose(skeleton, animation, sampleTime, localBindPose, localPose);
            AnimationRuntime::BuildGlobalPose(skeleton, localPose, globalPose);

            if (m_SelectedBone >= 0 && m_SelectedBone < localPose.Num())
            {
                AnimationRuntime::DecomposeTRS(localPose[m_SelectedBone], assetPoseLocalT, assetPoseLocalR, assetPoseLocalS);
                AnimationRuntime::DecomposeTRS(globalPose[m_SelectedBone], assetPoseGlobalT, assetPoseGlobalR, assetPoseGlobalS);

                assetPoseLocalR = FMath::normalize(assetPoseLocalR);
                assetPoseGlobalR = FMath::normalize(assetPoseGlobalR);
                assetPoseLocalEuler = FMath::degrees(FMath::eulerAngles(assetPoseLocalR));
                assetPoseGlobalEuler = FMath::degrees(FMath::eulerAngles(assetPoseGlobalR));
                hasAssetPoseData = true;
            }
        }
    }

    if (hasRuntimePoseData)
    {
        runtimeLocalT = skComp.RuntimeBoneLocalTranslations[m_SelectedBone];
        runtimeGlobalT = skComp.RuntimeBoneGlobalTranslations[m_SelectedBone];
        runtimeLocalS = skComp.RuntimeBoneLocalScales[m_SelectedBone];
        runtimeGlobalS = skComp.RuntimeBoneGlobalScales[m_SelectedBone];
        runtimeLocalR = FMath::normalize(skComp.RuntimeBoneLocalRotations[m_SelectedBone]);
        runtimeGlobalR = FMath::normalize(skComp.RuntimeBoneGlobalRotations[m_SelectedBone]);
        runtimeLocalEuler = FMath::degrees(FMath::eulerAngles(runtimeLocalR));
        runtimeGlobalEuler = FMath::degrees(FMath::eulerAngles(runtimeGlobalR));

        ImGui::Text("Runtime Local Translation:  %.3f  %.3f  %.3f",
            runtimeLocalT.x, runtimeLocalT.y, runtimeLocalT.z);
        ImGui::Text("Runtime Global Translation: %.3f  %.3f  %.3f",
            runtimeGlobalT.x, runtimeGlobalT.y, runtimeGlobalT.z);

        ImGui::Text("Runtime Local Scale:        %.3f  %.3f  %.3f",
            runtimeLocalS.x, runtimeLocalS.y, runtimeLocalS.z);
        ImGui::Text("Runtime Global Scale:       %.3f  %.3f  %.3f",
            runtimeGlobalS.x, runtimeGlobalS.y, runtimeGlobalS.z);

        ImGui::Text("Runtime Local Rotation Q:  %.3f  %.3f  %.3f  %.3f",
            runtimeLocalR.w, runtimeLocalR.x, runtimeLocalR.y, runtimeLocalR.z);
        ImGui::Text("Runtime Local Rotation E:  %.3f  %.3f  %.3f",
            runtimeLocalEuler.x, runtimeLocalEuler.y, runtimeLocalEuler.z);

        ImGui::Text("Runtime Global Rotation Q: %.3f  %.3f  %.3f  %.3f",
            runtimeGlobalR.w, runtimeGlobalR.x, runtimeGlobalR.y, runtimeGlobalR.z);
        ImGui::Text("Runtime Global Rotation E: %.3f  %.3f  %.3f",
            runtimeGlobalEuler.x, runtimeGlobalEuler.y, runtimeGlobalEuler.z);
    }
    else
    {
        ImGui::TextDisabled("Runtime pose data not available yet.");
    }

    ImGui::Separator();
    ImGui::Text("Asset Pose (Sampled)");

    if (hasAssetPoseData)
    {
        ImGui::Text("Asset Pose Local Translation:  %.3f  %.3f  %.3f",
            assetPoseLocalT.x, assetPoseLocalT.y, assetPoseLocalT.z);
        ImGui::Text("Asset Pose Global Translation: %.3f  %.3f  %.3f",
            assetPoseGlobalT.x, assetPoseGlobalT.y, assetPoseGlobalT.z);

        ImGui::Text("Asset Pose Local Scale:        %.3f  %.3f  %.3f",
            assetPoseLocalS.x, assetPoseLocalS.y, assetPoseLocalS.z);
        ImGui::Text("Asset Pose Global Scale:       %.3f  %.3f  %.3f",
            assetPoseGlobalS.x, assetPoseGlobalS.y, assetPoseGlobalS.z);

        ImGui::Text("Asset Pose Local Rotation Q:  %.3f  %.3f  %.3f  %.3f",
            assetPoseLocalR.w, assetPoseLocalR.x, assetPoseLocalR.y, assetPoseLocalR.z);
        ImGui::Text("Asset Pose Local Rotation E:  %.3f  %.3f  %.3f",
            assetPoseLocalEuler.x, assetPoseLocalEuler.y, assetPoseLocalEuler.z);

        ImGui::Text("Asset Pose Global Rotation Q: %.3f  %.3f  %.3f  %.3f",
            assetPoseGlobalR.w, assetPoseGlobalR.x, assetPoseGlobalR.y, assetPoseGlobalR.z);
        ImGui::Text("Asset Pose Global Rotation E: %.3f  %.3f  %.3f",
            assetPoseGlobalEuler.x, assetPoseGlobalEuler.y, assetPoseGlobalEuler.z);
    }
    else
    {
        ImGui::TextDisabled("Asset pose data not available.");
    }

    if (hasAssetPoseData && hasRuntimePoseData)
    {
        ImGui::Separator();
        ImGui::Text("Asset Pose Delta vs Runtime");

        const Vector3 deltaLocalT = assetPoseLocalT - runtimeLocalT;
        const Vector3 deltaGlobalT = assetPoseGlobalT - runtimeGlobalT;
        const Vector3 deltaLocalS = assetPoseLocalS - runtimeLocalS;
        const Vector3 deltaGlobalS = assetPoseGlobalS - runtimeGlobalS;
        const float deltaLocalQDeg = QuaternionAngularDifferenceDegrees(assetPoseLocalR, runtimeLocalR);
        const float deltaGlobalQDeg = QuaternionAngularDifferenceDegrees(assetPoseGlobalR, runtimeGlobalR);
        const Vector3 deltaLocalEuler = assetPoseLocalEuler - runtimeLocalEuler;
        const Vector3 deltaGlobalEuler = assetPoseGlobalEuler - runtimeGlobalEuler;

        ImGui::Text("Delta Local Translation:  %.3f  %.3f  %.3f (len %.3f)",
            deltaLocalT.x, deltaLocalT.y, deltaLocalT.z, FMath::length(deltaLocalT));
        ImGui::Text("Delta Global Translation: %.3f  %.3f  %.3f (len %.3f)",
            deltaGlobalT.x, deltaGlobalT.y, deltaGlobalT.z, FMath::length(deltaGlobalT));

        ImGui::Text("Delta Local Scale:        %.3f  %.3f  %.3f (len %.3f)",
            deltaLocalS.x, deltaLocalS.y, deltaLocalS.z, FMath::length(deltaLocalS));
        ImGui::Text("Delta Global Scale:       %.3f  %.3f  %.3f (len %.3f)",
            deltaGlobalS.x, deltaGlobalS.y, deltaGlobalS.z, FMath::length(deltaGlobalS));

        ImGui::Text("Delta Local Rotation Q:   %.3f deg", deltaLocalQDeg);
        ImGui::Text("Delta Global Rotation Q:  %.3f deg", deltaGlobalQDeg);
        ImGui::Text("Delta Local Rotation E:   %.3f  %.3f  %.3f",
            deltaLocalEuler.x, deltaLocalEuler.y, deltaLocalEuler.z);
        ImGui::Text("Delta Global Rotation E:  %.3f  %.3f  %.3f",
            deltaGlobalEuler.x, deltaGlobalEuler.y, deltaGlobalEuler.z);
    }

    ImGui::Separator();
    ImGui::Text("Asset Track Data (Raw)");

    if (!animation)
    {
        ImGui::TextDisabled("No animation asset selected.");
    }
    else
    {
        const AnimationTrack* track = animation->FindTrackForBone(m_SelectedBone);
        if (!track)
        {
            ImGui::TextDisabled("Selected bone has no track in this clip.");
        }
        else
        {
            const int32 positionKeyCount = static_cast<int32>(track->PositionKeys.Num());
            const int32 rotationKeyCount = static_cast<int32>(track->RotationKeys.Num());
            const int32 scaleKeyCount = static_cast<int32>(track->ScaleKeys.Num());

            ImGui::Text("Track Bone: %s", track->BoneName.c_str());
            ImGui::Text("Position Keys: %d", positionKeyCount);
            ImGui::Text("Rotation Keys: %d", rotationKeyCount);
            ImGui::Text("Scale Keys:    %d", scaleKeyCount);
            ImGui::Text("Sample Time: %.3f", sampleTime);

            if (positionKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Vector3 sampled =
                    SampleDebugAnimationVectorKeys(track->PositionKeys, sampleTime, &fromKey, &toKey, &alpha);

                ImGui::Text("Asset Track Translation (Sampled): %.3f  %.3f  %.3f", sampled.x, sampled.y, sampled.z);

                if (hasRuntimePoseData)
                {
                    const Vector3 localTranslationDelta = sampled - runtimeLocalT;
                    const float localTranslationDeltaLen = FMath::length(localTranslationDelta);
                    ImGui::Text("Delta vs Runtime Local T: %.3f  %.3f  %.3f (len %.3f)",
                        localTranslationDelta.x,
                        localTranslationDelta.y,
                        localTranslationDelta.z,
                        localTranslationDeltaLen);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationVecKey& from = track->PositionKeys[fromKey];
                    ImGui::Text("From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationVecKey& to = track->PositionKeys[toKey];
                        ImGui::Text("To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Translation Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        positionKeyCount < maxShownKeys ? positionKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationVecKey& key = track->PositionKeys[i];
                        ImGui::Text(
                            "#%d  t=%.3f  v=(%.3f, %.3f, %.3f)",
                            i, key.TimeSeconds, key.Value.x, key.Value.y, key.Value.z);
                    }

                    if (shownKeyCount < positionKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", positionKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No translation keys on this track.");
            }

            if (rotationKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Quaternion sampled =
                    SampleDebugAnimationRotationKeys(track->RotationKeys, sampleTime, &fromKey, &toKey, &alpha);
                const Vector3 sampledEuler = FMath::degrees(FMath::eulerAngles(sampled));

                ImGui::Text("Asset Track Rotation Q (Sampled): %.3f  %.3f  %.3f  %.3f",
                    sampled.w, sampled.x, sampled.y, sampled.z);
                ImGui::Text("Asset Track Rotation E (Sampled): %.3f  %.3f  %.3f",
                    sampledEuler.x, sampledEuler.y, sampledEuler.z);

                if (hasRuntimePoseData)
                {
                    const float localAngleDelta = QuaternionAngularDifferenceDegrees(sampled, runtimeLocalR);
                    const Vector3 localEulerDelta = sampledEuler - runtimeLocalEuler;

                    ImGui::Text("Delta vs Runtime Local Q: %.3f deg", localAngleDelta);
                    ImGui::Text("Delta vs Runtime Local E: %.3f  %.3f  %.3f",
                        localEulerDelta.x, localEulerDelta.y, localEulerDelta.z);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationQuatKey& from = track->RotationKeys[fromKey];
                    ImGui::Text("Rot From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationQuatKey& to = track->RotationKeys[toKey];
                        ImGui::Text("Rot To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Rotation Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        rotationKeyCount < maxShownKeys ? rotationKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationQuatKey& key = track->RotationKeys[i];
                        const Quaternion normalized = FMath::normalize(key.Value);
                        const Vector3 keyEuler = FMath::degrees(FMath::eulerAngles(normalized));
                        ImGui::Text(
                            "#%d  t=%.3f  q=(%.3f, %.3f, %.3f, %.3f)  e=(%.3f, %.3f, %.3f)",
                            i,
                            key.TimeSeconds,
                            normalized.w, normalized.x, normalized.y, normalized.z,
                            keyEuler.x, keyEuler.y, keyEuler.z);
                    }

                    if (shownKeyCount < rotationKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", rotationKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No rotation keys on this track.");
            }

            if (scaleKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Vector3 sampled =
                    SampleDebugAnimationVectorKeys(track->ScaleKeys, sampleTime, &fromKey, &toKey, &alpha);

                ImGui::Text("Asset Track Scale (Sampled): %.3f  %.3f  %.3f", sampled.x, sampled.y, sampled.z);

                if (hasRuntimePoseData)
                {
                    const Vector3 localScaleDelta = sampled - runtimeLocalS;
                    const float localScaleDeltaLen = FMath::length(localScaleDelta);
                    ImGui::Text("Delta vs Runtime Local S: %.3f  %.3f  %.3f (len %.3f)",
                        localScaleDelta.x,
                        localScaleDelta.y,
                        localScaleDelta.z,
                        localScaleDeltaLen);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationVecKey& from = track->ScaleKeys[fromKey];
                    ImGui::Text("Scale From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationVecKey& to = track->ScaleKeys[toKey];
                        ImGui::Text("Scale To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Scale Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        scaleKeyCount < maxShownKeys ? scaleKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationVecKey& key = track->ScaleKeys[i];
                        ImGui::Text(
                            "#%d  t=%.3f  s=(%.3f, %.3f, %.3f)",
                            i, key.TimeSeconds, key.Value.x, key.Value.y, key.Value.z);
                    }

                    if (shownKeyCount < scaleKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", scaleKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No scale keys on this track.");
            }
        }
    }

}

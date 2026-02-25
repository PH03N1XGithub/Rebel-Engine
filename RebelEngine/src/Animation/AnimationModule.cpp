#include "EnginePch.h"
#include "Animation/AnimationModule.h"

#include <unordered_set>

#include "Animation/AnimationAsset.h"
#include "Animation/AnimationDebug.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/SkeletalMeshAsset.h"
#include "Animation/SkeletonAsset.h"
#include "AssetManager/AssetManagerModule.h"
#include "BaseEngine.h"
#include "Components.h"
#include "Scene.h"

namespace
{
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
} // namespace

AnimationModule::AnimationModule()
{
    E_TickType = TickType::Tick;
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

void AnimationModule::UpdateAnimations(Scene* scene, float dt)
{
    if (!scene || !GEngine)
        return;

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
        return;

    AssetManager& assetManager = assetModule->GetManager();
    auto& reg = scene->GetRegistry();
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

            AnimationRuntime::EvaluateLocalPose(
                skeleton,
                animation,
                skComp->PlaybackTime,
                localBindPose,
                localPose);
            AnimationRuntime::BuildGlobalPose(skeleton, localPose, globalPose);
        }
        else
        {
            skComp->PlaybackTime = 0.0f;
        }

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
    if (!GEngine)
        return;

    UpdateAnimations(GEngine->GetActiveScene(), deltaTime);
}

void AnimationModule::Shutdown()
{
}


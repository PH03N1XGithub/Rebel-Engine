#pragma once

#include "AssetManager/BaseAsset.h"

struct AnimationVecKey
{
    Float TimeSeconds = 0.0f;
    Vector3 Value = Vector3(0.0f);
};

struct AnimationQuatKey
{
    Float TimeSeconds = 0.0f;
    Quaternion Value = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
};

struct AnimationTrack
{
    String BoneName;
    int32 BoneIndex = -1;

    TArray<AnimationVecKey> PositionKeys;
    TArray<AnimationQuatKey> RotationKeys;
    TArray<AnimationVecKey> ScaleKeys;
};

struct AnimationRootDriverData
{
    bool bEnabled = false;
    bool bAffectsTranslation = true;
    bool bAffectsRotation = false;
    bool bAffectsScale = false;
    String NodeName;

    TArray<AnimationVecKey> PositionKeys;
    TArray<AnimationQuatKey> RotationKeys;
    TArray<AnimationVecKey> ScaleKeys;
};

struct AnimationAsset : Asset
{
    REFLECTABLE_CLASS(AnimationAsset, Asset)

    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    const AnimationTrack* FindTrackForBone(int32 boneIndex) const;

    AssetHandle m_SkeletonID = 0;
    String m_ClipName;
    Float m_DurationSeconds = 0.0f;
    Float m_TicksPerSecond = 0.0f;
    TArray<AnimationTrack> m_Tracks;
    AnimationRootDriverData m_RootDriver;
};

REFLECT_CLASS(AnimationAsset, Asset)
{
    REFLECT_PROPERTY(AnimationAsset, m_SkeletonID,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimationAsset, m_ClipName,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimationAsset, m_DurationSeconds,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(AnimationAsset, m_TicksPerSecond,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
}
END_REFLECT_CLASS(AnimationAsset)

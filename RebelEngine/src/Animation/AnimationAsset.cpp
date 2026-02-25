#include "EnginePch.h"
#include "Animation/AnimationAsset.h"

namespace
{
    template<typename T>
    void WriteRawArray(BinaryWriter& ar, const TArray<T>& values)
    {
        const uint32 count = static_cast<uint32>(values.Num());
        ar << count;
        if (count > 0)
            ar.WriteBytes(values.Data(), count * sizeof(T));
    }

    template<typename T>
    void ReadRawArray(BinaryReader& ar, TArray<T>& values)
    {
        uint32 count = 0;
        ar >> count;
        values.Resize(count);
        if (count > 0)
            ar.ReadBytes(values.Data(), count * sizeof(T));
    }
}

void AnimationAsset::Serialize(BinaryWriter& ar)
{
    ar << m_SkeletonID;
    ar << m_ClipName;
    ar << m_DurationSeconds;
    ar << m_TicksPerSecond;

    const uint32 trackCount = static_cast<uint32>(m_Tracks.Num());
    ar << trackCount;

    for (const AnimationTrack& track : m_Tracks)
    {
        ar << track.BoneName;
        ar << track.BoneIndex;
        WriteRawArray(ar, track.PositionKeys);
        WriteRawArray(ar, track.RotationKeys);
        WriteRawArray(ar, track.ScaleKeys);
    }

    ar << m_RootDriver.bEnabled;
    ar << m_RootDriver.bAffectsTranslation;
    ar << m_RootDriver.bAffectsRotation;
    ar << m_RootDriver.bAffectsScale;
    ar << m_RootDriver.NodeName;
    WriteRawArray(ar, m_RootDriver.PositionKeys);
    WriteRawArray(ar, m_RootDriver.RotationKeys);
    WriteRawArray(ar, m_RootDriver.ScaleKeys);
}

void AnimationAsset::Deserialize(BinaryReader& ar)
{
    ar >> m_SkeletonID;
    ar >> m_ClipName;
    ar >> m_DurationSeconds;
    ar >> m_TicksPerSecond;

    uint32 trackCount = 0;
    ar >> trackCount;
    m_Tracks.Resize(trackCount);

    for (uint32 i = 0; i < trackCount; ++i)
    {
        AnimationTrack& track = m_Tracks[i];
        ar >> track.BoneName;
        ar >> track.BoneIndex;
        ReadRawArray(ar, track.PositionKeys);
        ReadRawArray(ar, track.RotationKeys);
        ReadRawArray(ar, track.ScaleKeys);
    }

    m_RootDriver.bEnabled = false;
    m_RootDriver.bAffectsTranslation = true;
    m_RootDriver.bAffectsRotation = false;
    m_RootDriver.bAffectsScale = false;
    m_RootDriver.NodeName = "";
    m_RootDriver.PositionKeys.Clear();
    m_RootDriver.RotationKeys.Clear();
    m_RootDriver.ScaleKeys.Clear();

    if (SerializedVersion >= 3)
    {
        ar >> m_RootDriver.bEnabled;
        ar >> m_RootDriver.bAffectsTranslation;
        ar >> m_RootDriver.bAffectsRotation;
        ar >> m_RootDriver.bAffectsScale;
        ar >> m_RootDriver.NodeName;
        ReadRawArray(ar, m_RootDriver.PositionKeys);
        ReadRawArray(ar, m_RootDriver.RotationKeys);
        ReadRawArray(ar, m_RootDriver.ScaleKeys);
    }
    else
    {
        // Backward compatibility for clips imported before root-driver metadata existed.
        constexpr int32 kLegacyRootDriverTrackIndex = -2;
        for (int32 i = 0; i < m_Tracks.Num(); ++i)
        {
            const AnimationTrack& legacyTrack = m_Tracks[i];
            if (legacyTrack.BoneIndex != kLegacyRootDriverTrackIndex)
                continue;

            m_RootDriver.bEnabled = true;
            m_RootDriver.bAffectsTranslation = true;
            m_RootDriver.bAffectsRotation = false;
            m_RootDriver.bAffectsScale = false;
            m_RootDriver.NodeName = legacyTrack.BoneName;
            m_RootDriver.PositionKeys = legacyTrack.PositionKeys;
            m_RootDriver.RotationKeys = legacyTrack.RotationKeys;
            m_RootDriver.ScaleKeys = legacyTrack.ScaleKeys;
            m_Tracks.RemoveAt(i);
            break;
        }
    }
}

void AnimationAsset::PostLoad()
{
    Asset::PostLoad();
}

const AnimationTrack* AnimationAsset::FindTrackForBone(int32 boneIndex) const
{
    for (const AnimationTrack& track : m_Tracks)
    {
        if (track.BoneIndex == boneIndex)
            return &track;
    }
    return nullptr;
}

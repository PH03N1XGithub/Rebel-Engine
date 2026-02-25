#include "EnginePch.h"
#include "Animation/SkeletonAsset.h"
#include <string>

namespace
{
    String MakeFallbackBoneName(int32 index)
    {
        const std::string value = "Bone_" + std::to_string(index);
        return String(value.c_str());
    }
}

void SkeletonAsset::Serialize(BinaryWriter& ar)
{
    ar << m_Parent;
    ar << m_InvBind;

    const uint32 nameCount = static_cast<uint32>(m_BoneNames.Num());
    ar << nameCount;
    for (uint32 i = 0; i < nameCount; ++i)
        ar << m_BoneNames[i];
}

void SkeletonAsset::Deserialize(BinaryReader& ar)
{
    ar >> m_Parent;
    ar >> m_InvBind;

    m_BoneNames.Clear();
    if (SerializedVersion >= 2)
    {
        uint32 nameCount = 0;
        ar >> nameCount;
        m_BoneNames.Resize(nameCount);
        for (uint32 i = 0; i < nameCount; ++i)
            ar >> m_BoneNames[i];
    }

    if (m_BoneNames.Num() != m_Parent.Num())
    {
        m_BoneNames.Resize(m_Parent.Num());
        for (int32 i = 0; i < m_BoneNames.Num(); ++i)
            m_BoneNames[i] = MakeFallbackBoneName(i);
    }
}

void SkeletonAsset::PostLoad()
{
    Asset::PostLoad();
}

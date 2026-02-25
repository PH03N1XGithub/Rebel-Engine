#include "EnginePch.h"
#include "Animation/SkeletalMeshAsset.h"

void SkeletalMeshAsset::Serialize(BinaryWriter& ar)
{
    ar << Vertices;
    ar << Indices;
    ar << m_Skeleton;
}

void SkeletalMeshAsset::Deserialize(BinaryReader& ar)
{
    ar >> Vertices;
    ar >> Indices;
    ar >> m_Skeleton;
}

void SkeletalMeshAsset::PostLoad()
{
    Asset::PostLoad();
}

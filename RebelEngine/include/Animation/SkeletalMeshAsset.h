#pragma once
#include "SkeletonAsset.h"
#include "AssetManager/AssetPtr.h"
#include "AssetManager/BaseAsset.h"
#include "AssetManager/MeshAsset.h"

struct SkeletalMeshAsset : Asset
{
    REFLECTABLE_CLASS(SkeletalMeshAsset, Asset)
    
    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    TArray<Vertex> Vertices;
    TArray<uint32> Indices;
    MeshHandle Handle;
    AssetPtr<SkeletonAsset>     m_Skeleton;
private:
    
};
REFLECT_CLASS(SkeletalMeshAsset, Asset)
END_REFLECT_CLASS(SkeletalMeshAsset)

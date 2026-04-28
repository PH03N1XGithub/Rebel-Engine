#pragma once
#include "SkeletonAsset.h"
#include "Engine/Assets/AssetPtr.h"
#include "Engine/Assets/BaseAsset.h"
#include "Engine/Assets/MeshAsset.h"

struct SkeletalMeshAsset : Asset
{
    REFLECTABLE_CLASS(SkeletalMeshAsset, Asset)
    
    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    AssetDisplayColor GetDisplayColor() const override { return GetStaticDisplayColor(); }

    static constexpr AssetDisplayColor GetStaticDisplayColor()
    {
        return { 165, 100, 255, 255 };
    }

    TArray<Vertex> Vertices;
    TArray<uint32> Indices;
    MeshHandle Handle;
    AssetPtr<SkeletonAsset>     m_Skeleton;
private:
    
};
REFLECT_CLASS(SkeletalMeshAsset, Asset)
END_REFLECT_CLASS(SkeletalMeshAsset)


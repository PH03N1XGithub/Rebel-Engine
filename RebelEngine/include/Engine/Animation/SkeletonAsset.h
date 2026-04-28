#pragma once
#include "Engine/Assets/BaseAsset.h"

struct SkeletonAsset : Asset 
{   
    REFLECTABLE_CLASS(SkeletonAsset, Asset)
    static constexpr uint32 kCurrentVersion = 2;

    SkeletonAsset()
    {
        SerializedVersion = kCurrentVersion;
    }

    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    AssetDisplayColor GetDisplayColor() const override { return GetStaticDisplayColor(); }

    static constexpr AssetDisplayColor GetStaticDisplayColor()
    {
        return { 115, 205, 255, 255 };
    }

    TArray<int32>   m_Parent;
    TArray<Mat4>    m_InvBind;
    TArray<String>  m_BoneNames;
private:

};
REFLECT_CLASS(SkeletonAsset, Asset)
END_REFLECT_CLASS(SkeletonAsset)


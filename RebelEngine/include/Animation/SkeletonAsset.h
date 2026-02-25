#pragma once
#include "AssetManager/BaseAsset.h"

struct SkeletonAsset : Asset 
{   
    REFLECTABLE_CLASS(SkeletonAsset, Asset)
    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;
    void PostLoad() override;

    TArray<int32>   m_Parent;
    TArray<Mat4>    m_InvBind;
    TArray<String>  m_BoneNames;
private:

};
REFLECT_CLASS(SkeletonAsset, Asset)
END_REFLECT_CLASS(SkeletonAsset)

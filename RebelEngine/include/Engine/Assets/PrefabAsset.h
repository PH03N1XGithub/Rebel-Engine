#pragma once

#include "Engine/Assets/BaseAsset.h"

struct PrefabAsset : Asset
{
    REFLECTABLE_CLASS(PrefabAsset, Asset)

    void Serialize(BinaryWriter& ar) override;
    void Deserialize(BinaryReader& ar) override;

    AssetDisplayColor GetDisplayColor() const override { return GetStaticDisplayColor(); }

    static constexpr AssetDisplayColor GetStaticDisplayColor()
    {
        return { 55, 145, 255, 255 };
    }

    String m_ActorTypeName;
    String m_TemplateYaml;
};

REFLECT_CLASS(PrefabAsset, Asset)
{
    REFLECT_PROPERTY(PrefabAsset, m_ActorTypeName,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
}
END_REFLECT_CLASS(PrefabAsset)

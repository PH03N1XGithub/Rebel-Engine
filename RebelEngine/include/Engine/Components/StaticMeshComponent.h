#pragma once

#include "Engine/Assets/AssetPtr.h"
#include "Engine/Assets/MeshAsset.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Rendering/Mesh.h"

struct StaticMeshComponent : SceneComponent
{
    AssetPtr<MeshAsset> Mesh{};
    MaterialHandle Material{};

    Bool bIsVisible = true;
    Bool bCastShadows = true;

    StaticMeshComponent() = default;

    StaticMeshComponent(AssetHandle meshAsset,
                        MaterialHandle mat = MaterialHandle(),
                        Bool visible = true,
                        Bool castShadows = true)
        : Mesh(meshAsset)
        , Material(mat)
        , bIsVisible(visible)
        , bCastShadows(castShadows)
    {}

    Bool IsValid() const
    {
        return (uint64)Mesh.GetHandle() != 0;
    }
    explicit operator Bool() const { return IsValid(); }

    REFLECTABLE_CLASS(StaticMeshComponent, SceneComponent)
};

REFLECT_CLASS(StaticMeshComponent, SceneComponent)
{
    REFLECT_PROPERTY(StaticMeshComponent, Mesh,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(StaticMeshComponent, Material,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(StaticMeshComponent, bIsVisible,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(StaticMeshComponent, bCastShadows,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(StaticMeshComponent)
REFLECT_ECS_COMPONENT(StaticMeshComponent)

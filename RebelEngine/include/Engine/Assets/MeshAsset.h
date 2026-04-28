#pragma once
#include "BaseAsset.h"

struct MeshAsset : Asset
{
	REFLECTABLE_CLASS(MeshAsset, Asset)
	TArray<Vertex> Vertices;
	TArray<uint32> Indices;
	MeshHandle Handle;

	void Serialize(BinaryWriter& ar) override;

	void Deserialize(BinaryReader& ar) override;

	void PostLoad() override;

	AssetDisplayColor GetDisplayColor() const override { return GetStaticDisplayColor(); }

	static constexpr AssetDisplayColor GetStaticDisplayColor()
	{
		return { 80, 210, 255, 255 };
	}
};
REFLECT_CLASS(MeshAsset, Asset)
	REFLECT_PROPERTY(MeshAsset,Vertices, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor );
REFLECT_PROPERTY(MeshAsset,Indices, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor );
//REFLECT_PROPERTY(MeshAsset,Handle, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor );
END_REFLECT_CLASS(MeshAsset)

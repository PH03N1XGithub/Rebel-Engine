#pragma once
#include "RenderModule.h"
#include "Core/AssetPtrBase.h"
#include "Core/GUID.h"


enum class AssetLoadState : uint8
{
	Unloaded,
	Loaded
};


struct AssetMeta
{
	AssetHandle    ID;
	const Rebel::Core::Reflection::TypeInfo*  Type;
	String     Path;
	uint64     FileSize;
	AssetLoadState State = AssetLoadState::Unloaded;
};


class Asset
{
	REFLECTABLE_CLASS(Asset, void)
public:
	AssetHandle  ID;
	String     Path;
	uint32	   SerializedVersion = 1;
	virtual ~Asset() = default;

	virtual void Serialize(BinaryWriter& ar) {}
	virtual void Deserialize(BinaryReader& ar) {}
	virtual void PostLoad() {}
};
REFLECT_CLASS(Asset, void)
	REFLECT_PROPERTY(Asset,ID, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
REFLECT_PROPERTY(Asset,Path,Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
END_REFLECT_CLASS(Asset)


inline bool IsValidAssetHandle(const AssetHandle& h)
{
	return static_cast<uint64>(h) != 0;   // or whatever your zero / null GUID is
}







// AssetManager.h
#pragma once
#include "AssetFileHeader.h"
#include "BaseAsset.h"
#include "AssetRegistry.h"
//#include "AssetManager/AssetPtr.h"

class AssetManager
{
public:
	AssetManager(AssetRegistry& registry)
		: m_Registry(registry) {}

	template<typename T>
	T* Create()
	{
		static_assert(std::is_base_of_v<Asset, T>);

		const auto* type = T::StaticType();
		if (!type || !type->CreateInstance)
			return nullptr;

		Asset* raw = static_cast<Asset*>(type->CreateInstance());

		//raw->ID   = AssetHandle::New();
		raw->Path = "Assets/" /*+ type->Name*/;

		// Own it
		m_AssetStorage.Add(RUniquePtr<Asset>(raw));

		// Index it
		m_LoadedAssets[raw->ID] = raw;

		return static_cast<T*>(raw);
	}

	/*Asset* AssetManager::Load(const AssetHandle& handle)
	{
		const AssetMeta* meta = m_Registry.Get(handle);
		if (!meta)
			return nullptr;

		// Create instance
		Asset* asset = static_cast<Asset*>(meta->Type->CreateInstance());
		asset->ID   = meta->ID;
		asset->Path = meta->Path;

		// Deserialize
		if (!DeserializeAsset(*asset, meta->Path + ".rasset"))
		{
			meta->State = AssetLoadState::Failed;
			Destroy(asset);
			return nullptr;
		}

		// Store ownership
		m_AssetStorage.PushBack(RUniquePtr<Asset>(asset));
		m_LoadedAssets.Insert(handle, asset);

		meta->State = AssetLoadState::Loaded;
		return asset;
	}*/



	Asset* Load(AssetHandle id)
	{
		if (Asset** ptr = m_LoadedAssets.Find(id))
			return *ptr;

		const AssetMeta* meta = m_Registry.Get(id);
		if (!meta || !meta->Type || !meta->Type->CreateInstance)
			return nullptr;

		PROFILE_SCOPE("Asset loaded '" + meta->Path+"'")
		Asset* raw = static_cast<Asset*>(meta->Type->CreateInstance());
		raw->ID   = meta->ID;
		raw->Path = meta->Path;

		// OPEN THE REAL FILE
		String fullPath = meta->Path + ".rasset";
		FileStream fs(fullPath.c_str(), "rb");
		BinaryReader ar(fs);

		// READ HEADER
		AssetFileHeader header;
		ar.Read(header);

		assert(header.Magic == AssetFileHeader::MagicValue);
		assert(header.AssetID == (uint64)meta->ID);
		raw->SerializedVersion = header.Version;

		// SEEK TO PAYLOAD
		ar.Seek(header.PayloadOffset);

		// NOW read the real asset
		raw->Deserialize(ar);
		raw->PostLoad();

		m_AssetStorage.Add(RUniquePtr<Asset>(raw));
		m_LoadedAssets[id] = raw;

		return raw;
	}

	

	Asset* Get(AssetHandle id)
	{
		if (Asset** ptr = m_LoadedAssets.Find(id))
			return *ptr;
		return nullptr;
	}
	bool IsLoaded(AssetHandle id) const
	{
		return m_LoadedAssets.Find(id) != nullptr;
	}


	void Unload(AssetHandle id)
	{
		Asset** ptr = m_LoadedAssets.Find(id);
		if (!ptr)
			return;

		Asset* asset = *ptr;

		// 1) Remove from lookup map
		m_LoadedAssets.Remove(id);

		// 2) Find and remove owning UniquePtr
		for (int i = 0; i < m_AssetStorage.Num(); ++i)
		{
			if (m_AssetStorage[i].Get() == asset)
			{
				m_AssetStorage.RemoveAt(i);   // UniquePtr destructor runs → deletes Asset
				return;
			}
		}
	}


	void Clear()
	{
		m_LoadedAssets.Clear();
		m_AssetStorage.Clear();
	}

private:
	AssetRegistry& m_Registry;
	TArray<Rebel::Core::Memory::UniquePtr<Asset>> m_AssetStorage;
	TMap<AssetHandle, Asset*> m_LoadedAssets;
};

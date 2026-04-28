// AssetRegistry.h
#pragma once
#include "BaseAsset.h"

DEFINE_LOG_CATEGORY(AssetRegistryLog)

class AssetRegistry
{
public:
	void Register(const AssetMeta& meta)
	{
		m_Registry[meta.ID] = meta;
		RB_LOG(AssetRegistryLog,info,"Asset found | ID={} | Type={} | Path={}",(uint64)meta.ID,meta.Type->Name,meta.Path)
	}

	const AssetMeta* Get(AssetHandle id) const
	{
		return m_Registry.Find(id);   // returns AssetMeta* or nullptr
	}

	const TMap<AssetHandle, AssetMeta>& GetAll() const
	{
		return m_Registry;
	}

	void Clear()
	{
		m_Registry.Clear();
	}

private:
	TMap<AssetHandle, AssetMeta> m_Registry;
};

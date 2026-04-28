#pragma once
#include "BaseAsset.h"
#include "Core/AssetPtrBase.h"


template<typename T>
class AssetPtr : public AssetPtrBase
{
public:
	AssetPtr() = default;
	AssetPtr(AssetHandle h)
	{
		Handle = h;
	}

	T* Get() const
	{
		if (IsValidAssetHandle(Handle))
		{
			if (Ptr)
				return Ptr;
		}
		return nullptr;
	};

	const Rebel::Core::Reflection::TypeInfo* GetAssetType() const override
	{
		return T::StaticType();
	}

	T* operator->() const { return Get(); }
	operator bool() const { return IsValidAssetHandle(Handle); }

	
	

private:
	
	mutable T* Ptr = nullptr;
};

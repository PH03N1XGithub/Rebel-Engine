#pragma once
#include "GUID.h"
#include "Reflection.h"
using AssetHandle = Rebel::Core::GUID;
class AssetPtrBase
{
public:
	virtual ~AssetPtrBase() = default;
	virtual const Rebel::Core::Reflection::TypeInfo* GetAssetType() const = 0;
	AssetHandle GetHandle() const { return Handle; }
	void SetHandle(AssetHandle h) { Handle = h; }
protected:
	AssetHandle Handle = 0;
};
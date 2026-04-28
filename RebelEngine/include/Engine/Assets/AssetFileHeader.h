#pragma once
#include <cstdint>

struct AssetFileHeader
{
	// 'RAST' = Rebel ASseT
	static constexpr uint32 MagicValue = 0x54534152; 

	uint32 Magic   = MagicValue;
	uint32 Version = 1;

	// Stable GUID for this asset (what AssetHandle stores)
	uint64 AssetID = 0;

	// Hash of reflected class name (e.g. "MeshAsset")
	uint64 TypeHash = 0;

	// Byte offset in file where serialized asset data begins
	uint64 PayloadOffset = 0;
};

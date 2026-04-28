#pragma once
#include "Engine/Rendering/Mesh.h"
#include <string>
#include <assimp/code/ColladaHelper.h>

#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/SkeletonAsset.h"

class MeshLoader
{
public:
	static bool LoadMeshFromFile(
		const String& path,
		TArray<Vertex>& outVertices,
		TArray<uint32>& outIndices);
	
	static bool LoadSkeletalMeshFromFile(
		const String& path,
		TArray<Vertex>& outVertices,
		TArray<uint32>& outIndices,
		SkeletonAsset& outSkeleton);

    static bool LoadAnimationClipsFromFile(
        const String& path,
        const SkeletonAsset& skeleton,
        TArray<AnimationAsset>& outAnimations,
        bool bTreatChannelsAsRelative = false);

};



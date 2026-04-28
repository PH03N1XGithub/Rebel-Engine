#include "Engine/Framework/EnginePch.h"
#include "Engine/Rendering/MeshLoader.h"
#include "Engine/Animation/AnimationDiagnostics.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef ANIM_IMPORT_TRANSLATION_TEST_SCALE
#define ANIM_IMPORT_TRANSLATION_TEST_SCALE 1.0f
#endif

static Vector3 ToGlm(const aiVector3D& v) { return {v.x, v.y, v.z}; }
static Vector2 ToGlm2(const aiVector3D& v) { return {v.x, v.y}; }
static Vector4 ToGlm4(const aiVector3D& v, float w) { return {v.x, v.y, v.z, w}; }
static Quaternion ToQuat(const aiQuaternion& q) { return Quaternion(q.w, q.x, q.y, q.z); }

static void DecomposeTRSForImport(const Mat4& m, Vector3& outTranslation, Quaternion& outRotation, Vector3& outScale)
{
    outTranslation = Vector3(m[3]);

    Vector3 axisX = Vector3(m[0]);
    Vector3 axisY = Vector3(m[1]);
    Vector3 axisZ = Vector3(m[2]);

    outScale.x = FMath::length(axisX);
    outScale.y = FMath::length(axisY);
    outScale.z = FMath::length(axisZ);

    axisX = (std::fabs(outScale.x) > 1e-6f) ? (axisX / outScale.x) : Vector3(1.0f, 0.0f, 0.0f);
    axisY = (std::fabs(outScale.y) > 1e-6f) ? (axisY / outScale.y) : Vector3(0.0f, 1.0f, 0.0f);
    axisZ = (std::fabs(outScale.z) > 1e-6f) ? (axisZ / outScale.z) : Vector3(0.0f, 0.0f, 1.0f);

    Mat3 rot(1.0f);
    rot[0] = axisX;
    rot[1] = axisY;
    rot[2] = axisZ;

    if (FMath::determinant(rot) < 0.0f)
    {
        outScale.z *= -1.0f;
        rot[2] *= -1.0f;
    }

    outRotation = FMath::normalize(FMath::quat_cast(rot));
}

static void LogMatrixForDiagnostics(const Mat4& m)
{
    ANIM_LOG("[" << m[0][0] << ", " << m[1][0] << ", " << m[2][0] << ", " << m[3][0] << "]");
    ANIM_LOG("[" << m[0][1] << ", " << m[1][1] << ", " << m[2][1] << ", " << m[3][1] << "]");
    ANIM_LOG("[" << m[0][2] << ", " << m[1][2] << ", " << m[2][2] << ", " << m[3][2] << "]");
    ANIM_LOG("[" << m[0][3] << ", " << m[1][3] << ", " << m[2][3] << ", " << m[3][3] << "]");
}

static float MatrixMaxAbsIdentityErrorForDiagnostics(const Mat4& m)
{
    const Mat4 identity(1.0f);
    float maxErr = 0.0f;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
            maxErr = FMath::max(maxErr, std::fabs(m[c][r] - identity[c][r]));
    }
    return maxErr;
}

static Mat4 ConvertMatrix(const aiMatrix4x4& m)
{
    Mat4 result;

    result[0][0] = m.a1; result[1][0] = m.a2; result[2][0] = m.a3; result[3][0] = m.a4;
    result[0][1] = m.b1; result[1][1] = m.b2; result[2][1] = m.b3; result[3][1] = m.b4;
    result[0][2] = m.c1; result[1][2] = m.c2; result[2][2] = m.c3; result[3][2] = m.c4;
    result[0][3] = m.d1; result[1][3] = m.d2; result[2][3] = m.d3; result[3][3] = m.d4;

    return result;
}

static bool TryGetSceneMetaNumber(const aiScene* scene, const char* key, double& outValue)
{
    if (!scene || !scene->mMetaData || !key)
        return false;

    aiMetadata* md = scene->mMetaData;
    for (unsigned int i = 0; i < md->mNumProperties; ++i)
    {
        if (std::string(md->mKeys[i].C_Str()) != key)
            continue;

        const aiMetadataEntry& entry = md->mValues[i];
        switch (entry.mType)
        {
            case AI_BOOL:
                outValue = (*reinterpret_cast<bool*>(entry.mData)) ? 1.0 : 0.0;
                return true;
            case AI_INT32:
                outValue = static_cast<double>(*reinterpret_cast<int32_t*>(entry.mData));
                return true;
            case AI_UINT64:
                outValue = static_cast<double>(*reinterpret_cast<uint64_t*>(entry.mData));
                return true;
            case AI_FLOAT:
                outValue = static_cast<double>(*reinterpret_cast<float*>(entry.mData));
                return true;
            case AI_DOUBLE:
                outValue = *reinterpret_cast<double*>(entry.mData);
                return true;
#ifdef AI_INT64
            case AI_INT64:
                outValue = static_cast<double>(*reinterpret_cast<int64_t*>(entry.mData));
                return true;
#endif
#ifdef AI_UINT32
            case AI_UINT32:
                outValue = static_cast<double>(*reinterpret_cast<uint32_t*>(entry.mData));
                return true;
#endif
#ifdef AI_AISTRING
            case AI_AISTRING:
            {
                const aiString& s = *reinterpret_cast<aiString*>(entry.mData);
                try
                {
                    outValue = std::stod(s.C_Str());
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
#endif
            default:
                return false;
        }
    }

    return false;
}

static Vector3 NormalizeSafe(const Vector3& v, const Vector3& fallback)
{
    const float lenSq = FMath::dot(v, v);
    if (lenSq <= 1e-12f)
        return fallback;
    return FMath::normalize(v);
}

static Mat4 BuildOrthonormalAxisFromMatrix(const Mat4& inM)
{
    Vector3 axisX = NormalizeSafe(Vector3(inM[0]), Vector3(1.0f, 0.0f, 0.0f));
    Vector3 axisY = NormalizeSafe(Vector3(inM[1]), Vector3(0.0f, 1.0f, 0.0f));
    Vector3 axisZ = NormalizeSafe(Vector3(inM[2]), Vector3(0.0f, 0.0f, 1.0f));

    axisZ = NormalizeSafe(FMath::cross(axisX, axisY), axisZ);
    axisY = NormalizeSafe(FMath::cross(axisZ, axisX), axisY);

    if (FMath::dot(FMath::cross(axisX, axisY), axisZ) < 0.0f)
        axisZ *= -1.0f;

    Mat4 out(1.0f);
    out[0] = Vector4(axisX, 0.0f);
    out[1] = Vector4(axisY, 0.0f);
    out[2] = Vector4(axisZ, 0.0f);
    out[3] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    return out;
}

static bool BuildAxisFromMetadata(const aiScene* scene, Mat4& outAxis)
{
    double upAxisD = 0.0, upSignD = 1.0;
    double frontAxisD = 0.0, frontSignD = 1.0;
    double coordAxisD = 0.0, coordSignD = 1.0;

    if (!TryGetSceneMetaNumber(scene, "UpAxis", upAxisD) ||
        !TryGetSceneMetaNumber(scene, "UpAxisSign", upSignD) ||
        !TryGetSceneMetaNumber(scene, "FrontAxis", frontAxisD) ||
        !TryGetSceneMetaNumber(scene, "FrontAxisSign", frontSignD) ||
        !TryGetSceneMetaNumber(scene, "CoordAxis", coordAxisD) ||
        !TryGetSceneMetaNumber(scene, "CoordAxisSign", coordSignD))
    {
        return false;
    }

    const int upAxis = static_cast<int>(upAxisD);
    const int frontAxis = static_cast<int>(frontAxisD);
    const int coordAxis = static_cast<int>(coordAxisD);

    if (upAxis < 0 || upAxis > 2 || frontAxis < 0 || frontAxis > 2 || coordAxis < 0 || coordAxis > 2)
        return false;

    if (upAxis == frontAxis || upAxis == coordAxis || frontAxis == coordAxis)
        return false;

    const int upSign = (upSignD < 0.0) ? -1 : 1;
    const int frontSign = (frontSignD < 0.0) ? -1 : 1;
    const int coordSign = (coordSignD < 0.0) ? -1 : 1;

    Vector3 srcCols[3] = {
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 0.0f)
    };

    // Engine convention: +X forward, +Y right, +Z up.
    srcCols[frontAxis] = Vector3(static_cast<float>(frontSign), 0.0f, 0.0f);
    srcCols[coordAxis] = Vector3(0.0f, static_cast<float>(coordSign), 0.0f);
    srcCols[upAxis]    = Vector3(0.0f, 0.0f, static_cast<float>(upSign));

    outAxis = Mat4(1.0f);
    outAxis[0] = Vector4(srcCols[0], 0.0f);
    outAxis[1] = Vector4(srcCols[1], 0.0f);
    outAxis[2] = Vector4(srcCols[2], 0.0f);
    outAxis[3] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    return true;
}

static Mat4 ComputeSourceToEngineTransform(const aiScene* scene)
{
    if (!scene)
        return Mat4(1.0f);

    Mat4 rootTransform(1.0f);
    if (scene->mRootNode)
        rootTransform = ConvertMatrix(scene->mRootNode->mTransformation);

    Mat4 axisOnly(1.0f);
    const bool hasAxisMetadata = BuildAxisFromMetadata(scene, axisOnly);

    if (!hasAxisMetadata)
    {
        axisOnly = BuildOrthonormalAxisFromMatrix(rootTransform);

        // Geometry heuristic fallback for identity roots that are likely Y-up.
        if (scene->mNumMeshes > 0)
        {
            Vector3 minV(FLT_MAX, FLT_MAX, FLT_MAX);
            Vector3 maxV(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (unsigned m = 0; m < scene->mNumMeshes; ++m)
            {
                aiMesh* mesh = scene->mMeshes[m];
                for (unsigned i = 0; i < mesh->mNumVertices; ++i)
                {
                    const Vector3 p = ToGlm(mesh->mVertices[i]);
                    minV.x = std::min(minV.x, p.x); minV.y = std::min(minV.y, p.y); minV.z = std::min(minV.z, p.z);
                    maxV.x = std::max(maxV.x, p.x); maxV.y = std::max(maxV.y, p.y); maxV.z = std::max(maxV.z, p.z);
                }
            }

            const Vector3 geomExtent = maxV - minV;
            const int dominantAxis =
                (geomExtent.x >= geomExtent.y && geomExtent.x >= geomExtent.z) ? 0 :
                (geomExtent.y >= geomExtent.x && geomExtent.y >= geomExtent.z) ? 1 : 2;

            const bool rootIsIdentity =
                std::fabs(rootTransform[0][0] - 1.0f) < 1e-4f && std::fabs(rootTransform[1][1] - 1.0f) < 1e-4f &&
                std::fabs(rootTransform[2][2] - 1.0f) < 1e-4f && std::fabs(rootTransform[3][3] - 1.0f) < 1e-4f &&
                std::fabs(rootTransform[0][1]) < 1e-4f && std::fabs(rootTransform[0][2]) < 1e-4f &&
                std::fabs(rootTransform[1][0]) < 1e-4f && std::fabs(rootTransform[1][2]) < 1e-4f &&
                std::fabs(rootTransform[2][0]) < 1e-4f && std::fabs(rootTransform[2][1]) < 1e-4f;

            const float xyMin = std::min(geomExtent.x, geomExtent.y);
            const float xyMax = std::max(geomExtent.x, geomExtent.y);
            const bool xyClose = (xyMax / std::max(1e-4f, xyMin)) < 1.25f;
            const bool zIsThin = geomExtent.z * 2.0f < xyMin;
            const bool likelyYUp = (dominantAxis == 1) || (xyClose && zIsThin);

            if (rootIsIdentity && likelyYUp)
            {
                Mat4 yUpToZUp(1.0f); // +90 deg around X: +Y -> +Z
                yUpToZUp[0] = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
                yUpToZUp[1] = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
                yUpToZUp[2] = Vector4(0.0f, -1.0f, 0.0f, 0.0f);
                yUpToZUp[3] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
                axisOnly = yUpToZUp;
            }
        }
    }

    double unitScaleFactor = 0.0;
    bool hasUnitScale = TryGetSceneMetaNumber(scene, "UnitScaleFactor", unitScaleFactor);
    if (!hasUnitScale)
        hasUnitScale = TryGetSceneMetaNumber(scene, "OriginalUnitScaleFactor", unitScaleFactor);

    float metersPerUnit = 1.0f;
    if (hasUnitScale && unitScaleFactor > 0.0)
        metersPerUnit = static_cast<float>(unitScaleFactor * 0.01);

    Mat4 unitScaleMat(1.0f);
    unitScaleMat[0][0] = metersPerUnit;
    unitScaleMat[1][1] = metersPerUnit;
    unitScaleMat[2][2] = metersPerUnit;

    return unitScaleMat * axisOnly;
}

static std::string StripAssimpFbxHelperSuffix(const std::string& name)
{
    static const char* kMarker = "_$AssimpFbx$_";
    const size_t markerPos = name.find(kMarker);
    if (markerPos == std::string::npos)
        return name;

    return name.substr(0, markerPos);
}

static std::string NormalizeImportPathUtf8(const String& path)
{
    return std::filesystem::u8path(path.c_str()).lexically_normal().string();
}

bool MeshLoader::LoadAnimationClipsFromFile(
    const String& path,
    const SkeletonAsset& skeleton,
    TArray<AnimationAsset>& outAnimations,
    bool bTreatChannelsAsRelative)
{
    outAnimations.Clear();

    if (skeleton.m_Parent.IsEmpty() || skeleton.m_InvBind.IsEmpty())
    {
        std::cerr << "[MeshLoader][Animation] Skeleton is empty.\n";
        return false;
    }

    if (skeleton.m_BoneNames.Num() != skeleton.m_Parent.Num())
    {
        std::cerr << "[MeshLoader][Animation] Skeleton bone names are missing. Reimport skeleton first.\n";
        return false;
    }

    Assimp::Importer importer;
    const std::string normalizedPath = NormalizeImportPathUtf8(path);
    const aiScene* scene = importer.ReadFile(normalizedPath.c_str(),
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights);

    if (!scene || scene->mNumAnimations == 0)
    {
        std::cerr << "[MeshLoader][Animation] Assimp failed or no animation clips: "
                  << importer.GetErrorString() << "\n";
        return false;
    }

    std::unordered_map<std::string, int32> boneNameToIndex;
    boneNameToIndex.reserve(static_cast<size_t>(skeleton.m_BoneNames.Num()));
    for (int32 i = 0; i < skeleton.m_BoneNames.Num(); ++i)
    {
        boneNameToIndex[skeleton.m_BoneNames[i].c_str()] = i;
    }

    const int32 boneCount = static_cast<int32>(skeleton.m_Parent.Num());
    if (boneCount <= 0 || skeleton.m_InvBind.Num() != skeleton.m_Parent.Num())
    {
        std::cerr << "[MeshLoader][Animation] Skeleton bind data mismatch.\n";
        return false;
    }

    TArray<Mat4> globalBindPose;
    TArray<Mat4> localBindPose;
    globalBindPose.Resize(boneCount);
    localBindPose.Resize(boneCount);

    for (int32 i = 0; i < boneCount; ++i)
        globalBindPose[i] = FMath::inverse(skeleton.m_InvBind[i]);

    for (int32 i = 0; i < boneCount; ++i)
    {
        const int32 parent = skeleton.m_Parent[i];
        if (parent >= 0 && parent < boneCount)
            localBindPose[i] = FMath::inverse(globalBindPose[parent]) * globalBindPose[i];
        else
            localBindPose[i] = globalBindPose[i];
    }

    const Mat4 sourceToEngine = ComputeSourceToEngineTransform(scene);
    const float translationScale = FMath::length(Vector3(sourceToEngine[0]));
    const float rootDriverTranslationScale = translationScale * ANIM_IMPORT_TRANSLATION_TEST_SCALE;

    std::cout << "[MeshLoader][Animation] source translation scale=" << translationScale
              << " relativeMode=" << (bTreatChannelsAsRelative ? 1 : 0) << "\n";

    double unitScaleMeta = 1.0;
    const bool hasUnitScaleMeta =
        TryGetSceneMetaNumber(scene, "UnitScaleFactor", unitScaleMeta) ||
        TryGetSceneMetaNumber(scene, "OriginalUnitScaleFactor", unitScaleMeta);
    float unitScaleFactor = hasUnitScaleMeta ? static_cast<float>(unitScaleMeta) : 1.0f;
    if (!std::isfinite(unitScaleFactor) || unitScaleFactor <= 0.0f)
        unitScaleFactor = 1.0f;
    const float metersPerUnit = unitScaleFactor * 0.01f;

    std::cout << "[MeshLoader][Animation] FBX UnitScaleFactor = " << unitScaleFactor << "\n";
    std::cout << "[MeshLoader][Animation] FBX metersPerUnit = " << metersPerUnit << "\n";
    std::cout << "[MeshLoader][Animation] channel import space = skeleton_local\n";
    std::cout << "[MeshLoader][Animation] root driver translation scale = " << rootDriverTranslationScale << "\n";
    if (std::fabs(ANIM_IMPORT_TRANSLATION_TEST_SCALE - 1.0f) > 1e-6f)
    {
        std::cout << "[MeshLoader][Animation] WARNING translation test scale override enabled: "
                  << ANIM_IMPORT_TRANSLATION_TEST_SCALE << "\n";
    }

    Mat4 rootTransform(1.0f);
    if (scene->mRootNode)
        rootTransform = ConvertMatrix(scene->mRootNode->mTransformation);

    Vector3 rootTranslation(0.0f);
    Quaternion rootRotation(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 rootScale(1.0f);
    DecomposeTRSForImport(rootTransform, rootTranslation, rootRotation, rootScale);

    ANIM_LOG("=== FBX GLOBAL SETTINGS ===");
    ANIM_LOG("Context: Animation Import");
    ANIM_LOG("Source: " << path.c_str());
    ANIM_LOG("UnitScaleFactor: " << unitScaleFactor);
    ANIM_LOG("MetersPerUnit: " << metersPerUnit);
    ANIM_LOG("SceneScale(translationScale): " << translationScale);
    ANIM_LOG("RootDriverTranslationScale: " << rootDriverTranslationScale);
    ANIM_LOG("ChannelImportSpace: skeleton_local");
    ANIM_LOG("TranslationTestScaleOverride: " << ANIM_IMPORT_TRANSLATION_TEST_SCALE);
    ANIM_LOG("RootNode Scale: (" << rootScale.x << ", " << rootScale.y << ", " << rootScale.z << ")");
    ANIM_LOG("RootNode Transform:");
    LogMatrixForDiagnostics(rootTransform);
    ANIM_LOG("SourceToEngine:");
    LogMatrixForDiagnostics(sourceToEngine);
    ANIM_LOG_FLUSH();

    auto BuildClipName = [](const aiAnimation* anim, uint32 index) -> String
    {
        if (anim && anim->mName.length > 0)
            return String(anim->mName.C_Str());

        const std::string fallback = "Clip_" + std::to_string(index);
        return String(fallback.c_str());
    };

    std::unordered_map<std::string, std::string> nodeParentByName;
    std::unordered_map<std::string, const aiNode*> nodeByName;
    if (scene->mRootNode)
    {
        std::function<void(aiNode*, const std::string&)> gatherNodeParents =
            [&](aiNode* node, const std::string& parentName)
        {
            if (!node)
                return;

            const std::string nodeName = node->mName.C_Str();
            nodeByName[nodeName] = node;
            if (!parentName.empty())
                nodeParentByName[nodeName] = parentName;

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                gatherNodeParents(node->mChildren[c], nodeName);
        };

        gatherNodeParents(scene->mRootNode, std::string());
    }

    auto IsAncestorNodeName = [&](const std::string& ancestorName, const std::string& nodeName) -> bool
    {
        auto it = nodeParentByName.find(nodeName);
        while (it != nodeParentByName.end())
        {
            if (it->second == ancestorName)
                return true;

            it = nodeParentByName.find(it->second);
        }
        return false;
    };

    for (uint32 animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
    {
        aiAnimation* srcAnim = scene->mAnimations[animIndex];
        if (!srcAnim)
            continue;

        AnimationAsset clip;
        clip.m_SkeletonID = skeleton.ID;
        clip.m_ClipName = BuildClipName(srcAnim, animIndex);

        const double ticksPerSecond = (srcAnim->mTicksPerSecond > 0.0) ? srcAnim->mTicksPerSecond : 25.0;
        clip.m_TicksPerSecond = static_cast<float>(ticksPerSecond);
        clip.m_DurationSeconds =
            (ticksPerSecond > 0.0)
            ? static_cast<float>(srcAnim->mDuration / ticksPerSecond)
            : 0.0f;

        int32 mappedChannelCount = 0;
        int32 skippedChannelCount = 0;
        std::unordered_set<std::string> skippedChannelNames;
        std::unordered_set<std::string> mappedChannelNames;
        std::unordered_map<int32, int32> trackIndexByBoneIndex;
        struct UnmappedChannel
        {
            std::string Name;
            aiNodeAnim* Channel = nullptr;
        };
        std::vector<UnmappedChannel> unmappedChannels;

        auto FillChannelKeys = [&](TArray<AnimationVecKey>& outPositionKeys,
                                   TArray<AnimationQuatKey>& outRotationKeys,
                                   TArray<AnimationVecKey>& outScaleKeys,
                                   aiNodeAnim* channel,
                                   const char* debugBoneName)
        {
            if (!channel)
                return;

            outPositionKeys.Resize(channel->mNumPositionKeys);
            for (uint32 keyIndex = 0; keyIndex < channel->mNumPositionKeys; ++keyIndex)
            {
                const aiVectorKey& key = channel->mPositionKeys[keyIndex];
                AnimationVecKey& outKey = outPositionKeys[keyIndex];

                outKey.TimeSeconds = static_cast<float>(key.mTime / ticksPerSecond);
                Vector3 translation = ToGlm(key.mValue);
                translation *= ANIM_IMPORT_TRANSLATION_TEST_SCALE;
                outKey.Value = translation;

                ANIM_LOG("Bone " << (debugBoneName ? debugBoneName : "<Unknown>")
                         << " key pos imported: "
                         << translation.x << " " << translation.y << " " << translation.z
                         << " (keyIndex=" << keyIndex << ")");
            }

            outRotationKeys.Resize(channel->mNumRotationKeys);
            for (uint32 keyIndex = 0; keyIndex < channel->mNumRotationKeys; ++keyIndex)
            {
                const aiQuatKey& key = channel->mRotationKeys[keyIndex];
                AnimationQuatKey& outKey = outRotationKeys[keyIndex];

                outKey.TimeSeconds = static_cast<float>(key.mTime / ticksPerSecond);
                outKey.Value = FMath::normalize(ToQuat(key.mValue));
            }

            outScaleKeys.Resize(channel->mNumScalingKeys);
            for (uint32 keyIndex = 0; keyIndex < channel->mNumScalingKeys; ++keyIndex)
            {
                const aiVectorKey& key = channel->mScalingKeys[keyIndex];
                AnimationVecKey& outKey = outScaleKeys[keyIndex];

                outKey.TimeSeconds = static_cast<float>(key.mTime / ticksPerSecond);
                outKey.Value = ToGlm(key.mValue);
            }
        };

        auto FillTrackFromChannel = [&](AnimationTrack& track, aiNodeAnim* channel)
        {
            FillChannelKeys(track.PositionKeys, track.RotationKeys, track.ScaleKeys, channel, track.BoneName.c_str());
        };

        auto FillRootDriverFromChannel = [&](AnimationRootDriverData& rootDriver, aiNodeAnim* channel)
        {
            FillChannelKeys(rootDriver.PositionKeys, rootDriver.RotationKeys, rootDriver.ScaleKeys, channel, rootDriver.NodeName.c_str());

            if (std::fabs(rootDriverTranslationScale - 1.0f) > 1e-6f)
            {
                for (int32 keyIndex = 0; keyIndex < rootDriver.PositionKeys.Num(); ++keyIndex)
                    rootDriver.PositionKeys[keyIndex].Value *= rootDriverTranslationScale;
            }
        };

        for (uint32 c = 0; c < srcAnim->mNumChannels; ++c)
        {
            aiNodeAnim* channel = srcAnim->mChannels[c];
            if (!channel)
                continue;

            const std::string boneName = channel->mNodeName.C_Str();
            auto boneIt = boneNameToIndex.find(boneName);
            std::string resolvedBoneName = boneName;
            if (boneIt == boneNameToIndex.end())
            {
                const std::string strippedBoneName = StripAssimpFbxHelperSuffix(boneName);
                if (strippedBoneName != boneName)
                {
                    const auto strippedIt = boneNameToIndex.find(strippedBoneName);
                    if (strippedIt != boneNameToIndex.end())
                    {
                        boneIt = strippedIt;
                        resolvedBoneName = strippedBoneName;

                        std::cout << "[MeshLoader][Animation] Remapped helper channel '" << boneName
                                  << "' to bone '" << resolvedBoneName << "'.\n";
                    }
                }
            }

            if (boneIt == boneNameToIndex.end())
            {
                ++skippedChannelCount;
                skippedChannelNames.insert(boneName);
                unmappedChannels.push_back({boneName, channel});
                continue;
            }

            ++mappedChannelCount;
            mappedChannelNames.insert(resolvedBoneName);

            AnimationTrack incomingTrack;
            incomingTrack.BoneName = String(resolvedBoneName.c_str());
            incomingTrack.BoneIndex = boneIt->second;
            FillTrackFromChannel(incomingTrack, channel);

            // Bake skipped non-bone parent transforms into this track so channel local
            // space matches our collapsed skeleton hierarchy.
            const int32 trackBoneIndex = incomingTrack.BoneIndex;
            if (trackBoneIndex >= 0 && trackBoneIndex < skeleton.m_Parent.Num())
            {
                const auto nodeIt = nodeByName.find(boneName);
                if (nodeIt != nodeByName.end())
                {
                    std::string mappedParentName;
                    const int32 mappedParentIndex = skeleton.m_Parent[trackBoneIndex];
                    if (mappedParentIndex >= 0 && mappedParentIndex < skeleton.m_BoneNames.Num())
                        mappedParentName = skeleton.m_BoneNames[mappedParentIndex].c_str();

                    std::vector<const aiNode*> bridgeNodes;
                    bridgeNodes.reserve(8);

                    const aiNode* cursor = nodeIt->second->mParent;
                    bool reachedCollapsedParent = mappedParentName.empty();

                    while (cursor)
                    {
                        const std::string cursorName = cursor->mName.C_Str();
                        const bool isSceneRoot = (cursor->mParent == nullptr);

                        if (!mappedParentName.empty() && cursorName == mappedParentName)
                        {
                            reachedCollapsedParent = true;
                            break;
                        }

                        if (mappedParentName.empty() && isSceneRoot)
                        {
                            reachedCollapsedParent = true;
                            break;
                        }

                        bridgeNodes.push_back(cursor);
                        cursor = cursor->mParent;
                    }

                    if (reachedCollapsedParent && !bridgeNodes.empty())
                    {
                        Mat4 sourceBridge(1.0f);
                        for (int32 n = static_cast<int32>(bridgeNodes.size()) - 1; n >= 0; --n)
                            sourceBridge = sourceBridge * ConvertMatrix(bridgeNodes[n]->mTransformation);

                        Vector3 sourceBridgeTranslation(0.0f);
                        Quaternion sourceBridgeRotation(1.0f, 0.0f, 0.0f, 0.0f);
                        Vector3 sourceBridgeScale(1.0f);
                        DecomposeTRSForImport(sourceBridge, sourceBridgeTranslation, sourceBridgeRotation, sourceBridgeScale);

                        Vector3 bridgeTranslation = sourceBridgeTranslation;
                        bridgeTranslation *= ANIM_IMPORT_TRANSLATION_TEST_SCALE;

                        const Quaternion bridgeRotation = FMath::normalize(sourceBridgeRotation);

                        const bool hasBridgeScale =
                            FMath::length(sourceBridgeScale - Vector3(1.0f)) > 1e-3f;

                        // Ignore skipped-parent scale. Non-deforming bridge nodes (e.g. Armature)
                        // frequently contain unit-conversion scale that should not be applied to
                        // deforming bone channels.
                        const Mat4 engineBridgeNoScale =
                            FMath::translate(Mat4(1.0f), bridgeTranslation) * FMath::mat4_cast(bridgeRotation);

                        for (int32 keyIndex = 0; keyIndex < incomingTrack.PositionKeys.Num(); ++keyIndex)
                        {
                            const Vector4 transformed =
                                engineBridgeNoScale * Vector4(incomingTrack.PositionKeys[keyIndex].Value, 1.0f);
                            incomingTrack.PositionKeys[keyIndex].Value = Vector3(transformed);
                        }

                        for (int32 keyIndex = 0; keyIndex < incomingTrack.RotationKeys.Num(); ++keyIndex)
                        {
                            const Quaternion localQ = FMath::normalize(incomingTrack.RotationKeys[keyIndex].Value);
                            incomingTrack.RotationKeys[keyIndex].Value = FMath::normalize(bridgeRotation * localQ);
                        }

                        std::cout << "[MeshLoader][Animation] Track '" << incomingTrack.BoneName.c_str()
                                  << "' baked " << bridgeNodes.size()
                                  << " skipped parent transform(s) into channel data.\n";

                        if (hasBridgeScale)
                        {
                            std::cout << "[MeshLoader][Animation] Track '" << incomingTrack.BoneName.c_str()
                                      << "' ignored skipped-parent scale during bake ("
                                      << sourceBridgeScale.x << ", " << sourceBridgeScale.y << ", " << sourceBridgeScale.z
                                      << ").\n";
                        }
                    }
                    else if (!reachedCollapsedParent && !mappedParentName.empty())
                    {
                        std::cerr << "[MeshLoader][Animation] Track '" << incomingTrack.BoneName.c_str()
                                  << "' could not resolve collapsed parent '" << mappedParentName
                                  << "' in source hierarchy; channel may be misaligned.\n";
                    }
                }
            }

            if (bTreatChannelsAsRelative && trackBoneIndex >= 0 && trackBoneIndex < localBindPose.Num())
            {
                Vector3 bindTranslation(0.0f);
                Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
                Vector3 bindScale(1.0f);
                DecomposeTRSForImport(localBindPose[trackBoneIndex], bindTranslation, bindRotation, bindScale);

                for (int32 keyIndex = 0; keyIndex < incomingTrack.PositionKeys.Num(); ++keyIndex)
                    incomingTrack.PositionKeys[keyIndex].Value = bindTranslation + incomingTrack.PositionKeys[keyIndex].Value;

                for (int32 keyIndex = 0; keyIndex < incomingTrack.RotationKeys.Num(); ++keyIndex)
                {
                    const Quaternion relQ = FMath::normalize(incomingTrack.RotationKeys[keyIndex].Value);
                    incomingTrack.RotationKeys[keyIndex].Value = FMath::normalize(bindRotation * relQ);
                }

                for (int32 keyIndex = 0; keyIndex < incomingTrack.ScaleKeys.Num(); ++keyIndex)
                    incomingTrack.ScaleKeys[keyIndex].Value = bindScale * incomingTrack.ScaleKeys[keyIndex].Value;

                std::cout << "[MeshLoader][Animation] Track '" << incomingTrack.BoneName.c_str()
                          << "' converted from explicit relative mode to absolute local keys.\n";
            }

            AnimationTrack* debugTrack = &incomingTrack;
            const auto existingTrackIt = trackIndexByBoneIndex.find(incomingTrack.BoneIndex);
            if (existingTrackIt != trackIndexByBoneIndex.end())
            {
                AnimationTrack& existingTrack = clip.m_Tracks[existingTrackIt->second];

                auto MergeVecKeys = [](TArray<AnimationVecKey>& dst, const TArray<AnimationVecKey>& src)
                {
                    if (src.Num() > dst.Num())
                        dst = src;
                };

                auto MergeQuatKeys = [](TArray<AnimationQuatKey>& dst, const TArray<AnimationQuatKey>& src)
                {
                    if (src.Num() > dst.Num())
                        dst = src;
                };

                MergeVecKeys(existingTrack.PositionKeys, incomingTrack.PositionKeys);
                MergeQuatKeys(existingTrack.RotationKeys, incomingTrack.RotationKeys);
                MergeVecKeys(existingTrack.ScaleKeys, incomingTrack.ScaleKeys);

                std::cout << "[MeshLoader][Animation] Merged duplicate channel '" << boneName
                          << "' into track '" << existingTrack.BoneName.c_str() << "'.\n";
                debugTrack = &existingTrack;
            }
            else
            {
                clip.m_Tracks.Add(std::move(incomingTrack));
                trackIndexByBoneIndex[clip.m_Tracks.Back().BoneIndex] = clip.m_Tracks.Num() - 1;
                debugTrack = &clip.m_Tracks.Back();
            }

            ANIM_LOG("=== ANIMATION TRACK ===");
            ANIM_LOG("Track Bone: " << debugTrack->BoneName.c_str());
            ANIM_LOG("BoneIndex: " << debugTrack->BoneIndex);
            ANIM_LOG("PositionKeys: " << debugTrack->PositionKeys.Num());
            ANIM_LOG("RotationKeys: " << debugTrack->RotationKeys.Num());
            ANIM_LOG("ScaleKeys: " << debugTrack->ScaleKeys.Num());

            if (!debugTrack->PositionKeys.IsEmpty())
            {
                const AnimationVecKey& firstPos = debugTrack->PositionKeys[0];
                ANIM_LOG("First Pos Key: (" << firstPos.Value.x << ", " << firstPos.Value.y << ", " << firstPos.Value.z << ")");
            }
            if (!debugTrack->RotationKeys.IsEmpty())
            {
                const AnimationQuatKey& firstRot = debugTrack->RotationKeys[0];
                ANIM_LOG("First Rot Key: (" << firstRot.Value.x << ", " << firstRot.Value.y << ", " << firstRot.Value.z << ", " << firstRot.Value.w << ")");
            }
            if (!debugTrack->ScaleKeys.IsEmpty())
            {
                const AnimationVecKey& firstScale = debugTrack->ScaleKeys[0];
                ANIM_LOG("First Scale Key: (" << firstScale.Value.x << ", " << firstScale.Value.y << ", " << firstScale.Value.z << ")");
            }

            if (debugTrack->BoneIndex >= 0 && debugTrack->BoneIndex < localBindPose.Num() && !debugTrack->PositionKeys.IsEmpty())
            {
                Vector3 bindTranslation(0.0f);
                Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
                Vector3 bindScale(1.0f);
                DecomposeTRSForImport(localBindPose[debugTrack->BoneIndex], bindTranslation, bindRotation, bindScale);

                const float bindLength = FMath::length(bindTranslation);
                const float animLength = FMath::length(debugTrack->PositionKeys[0].Value);

                float scaleRatio = 1.0f;
                if (bindLength <= 1e-6f || animLength <= 1e-6f)
                    scaleRatio = (FMath::max(bindLength, animLength) <= 1e-6f) ? 1.0f : FLT_MAX;
                else
                    scaleRatio = FMath::max(bindLength, animLength) / FMath::min(bindLength, animLength);

                ANIM_LOG("Bind length: " << bindLength);
                ANIM_LOG("Anim key length: " << animLength);
                ANIM_LOG("Scale ratio: " << scaleRatio);

                if (scaleRatio > 10.0f)
                {
                    ANIM_LOG("WARNING: animation translation differs from bind by scale ratio > 10 for bone '"
                             << debugTrack->BoneName.c_str() << "' ratio=" << scaleRatio);
                }
            }
            ANIM_LOG_FLUSH();
        }

        aiNodeAnim* rootDriverChannel = nullptr;
        std::string rootDriverName;
        int32 mappedRootDriverTrackIndex = -1;

        auto ToLowerCopy = [](const std::string& s) -> std::string
        {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return out;
        };

        auto IsPreferredMappedRootDriverName = [](const std::string& nameLower) -> bool
        {
            // Be conservative when promoting a mapped bone track into root motion.
            // Mixamo often animates locomotion on hips/pelvis; stripping that translation
            // from the bone track produces broken-looking clips unless there is a true
            // authored root. Only treat explicit root-style names as safe here.
            static const char* kPreferredTokens[] = {
                "armature", "root"
            };

            for (const char* token : kPreferredTokens)
            {
                if (nameLower.find(token) != std::string::npos)
                    return true;
            }
            return false;
        };

        auto IsMixamoLocomotionCarrierName = [](const std::string& nameLower) -> bool
        {
            return nameLower.find("hips") != std::string::npos ||
                   nameLower.find("pelvis") != std::string::npos;
        };

        auto TrackHasTranslationMotion = [](const AnimationTrack& track) -> bool
        {
            if (track.PositionKeys.Num() < 2)
                return false;

            const Vector3 base = track.PositionKeys[0].Value;
            for (int32 i = 1; i < track.PositionKeys.Num(); ++i)
            {
                if (FMath::length(track.PositionKeys[i].Value - base) > 1e-4f)
                    return true;
            }
            return false;
        };

        auto ComputeBoneDepth = [&](int32 boneIndex) -> int32
        {
            if (boneIndex < 0 || boneIndex >= skeleton.m_Parent.Num())
                return INT_MAX;

            int32 depth = 0;
            int32 cursor = boneIndex;
            int32 guard = skeleton.m_Parent.Num() + 1;
            while (cursor >= 0 && cursor < skeleton.m_Parent.Num() && guard-- > 0)
            {
                ++depth;
                cursor = skeleton.m_Parent[cursor];
            }
            return depth;
        };

        for (const UnmappedChannel& unmapped : unmappedChannels)
        {
            if (unmapped.Name == "Armature")
            {
                rootDriverChannel = unmapped.Channel;
                rootDriverName = unmapped.Name;
                break;
            }
        }

        if (!rootDriverChannel)
        {
            for (const UnmappedChannel& unmapped : unmappedChannels)
            {
                bool drivesMappedBones = false;
                for (const std::string& mappedName : mappedChannelNames)
                {
                    if (IsAncestorNodeName(unmapped.Name, mappedName))
                    {
                        drivesMappedBones = true;
                        break;
                    }
                }

                if (drivesMappedBones)
                {
                    rootDriverChannel = unmapped.Channel;
                    rootDriverName = unmapped.Name;
                    break;
                }
            }
        }

        // Fallback: if all channels are mapped into bones, only pick an explicitly
        // root-like mapped track. Do not promote arbitrary shallow moving bones
        // like pelvis/hips into root motion, because that causes in-place clips
        // to drift and breaks foot planting.
        if (!rootDriverChannel)
        {
            int32 bestPreferredDepth = INT_MAX;
            for (int32 i = 0; i < clip.m_Tracks.Num(); ++i)
            {
                const AnimationTrack& track = clip.m_Tracks[i];
                const std::string nameLower = ToLowerCopy(track.BoneName.c_str());
                if (!IsPreferredMappedRootDriverName(nameLower))
                    continue;

                if (!TrackHasTranslationMotion(track))
                    continue;

                const int32 depth = ComputeBoneDepth(track.BoneIndex);
                if (depth < bestPreferredDepth)
                {
                    bestPreferredDepth = depth;
                    mappedRootDriverTrackIndex = i;
                }
            }

            if (mappedRootDriverTrackIndex < 0)
            {
                for (int32 i = 0; i < clip.m_Tracks.Num(); ++i)
                {
                    const AnimationTrack& track = clip.m_Tracks[i];
                    const std::string nameLower = ToLowerCopy(track.BoneName.c_str());
                    if (!IsMixamoLocomotionCarrierName(nameLower))
                        continue;

                    if (!TrackHasTranslationMotion(track))
                        continue;

                    const int32 parentIndex = (track.BoneIndex >= 0 && track.BoneIndex < skeleton.m_Parent.Num())
                        ? skeleton.m_Parent[track.BoneIndex]
                        : -1;
                    if (parentIndex < 0 || parentIndex >= skeleton.m_BoneNames.Num())
                        continue;

                    const int32 grandParentIndex = skeleton.m_Parent[parentIndex];
                    if (grandParentIndex >= 0)
                        continue;

                    const std::string parentNameLower = ToLowerCopy(skeleton.m_BoneNames[parentIndex].c_str());
                    if (parentNameLower.find("root") == std::string::npos)
                        continue;

                    mappedRootDriverTrackIndex = i;
                    std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                              << "' promoting Mixamo locomotion carrier '" << track.BoneName.c_str()
                              << "' to mapped root driver via parent '" << skeleton.m_BoneNames[parentIndex].c_str()
                              << "'.\n";
                    break;
                }
            }
        }

        clip.m_RootDriver.NodeName = "";
        clip.m_RootDriver.PositionKeys.Clear();
        clip.m_RootDriver.RotationKeys.Clear();
        clip.m_RootDriver.ScaleKeys.Clear();
        clip.m_RootDriver.bEnabled = false;
        clip.m_RootDriver.bAffectsTranslation = true;
        clip.m_RootDriver.bAffectsRotation = false;
        clip.m_RootDriver.bAffectsScale = false;

        if (rootDriverChannel)
        {
            clip.m_RootDriver.bEnabled = true;
            clip.m_RootDriver.NodeName = String(rootDriverName.c_str());
            FillRootDriverFromChannel(clip.m_RootDriver, rootDriverChannel);

            ++mappedChannelCount;
            if (skippedChannelCount > 0)
                --skippedChannelCount;
            skippedChannelNames.erase(rootDriverName);

            std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                      << "' applying root driver channel '" << rootDriverName << "'.\n";
        }
        else if (mappedRootDriverTrackIndex >= 0 && mappedRootDriverTrackIndex < clip.m_Tracks.Num())
        {
            AnimationTrack& mappedRootTrack = clip.m_Tracks[mappedRootDriverTrackIndex];
            clip.m_RootDriver.bEnabled = true;
            clip.m_RootDriver.NodeName = mappedRootTrack.BoneName;
            clip.m_RootDriver.PositionKeys = mappedRootTrack.PositionKeys;

            if (std::fabs(rootDriverTranslationScale - 1.0f) > 1e-6f)
            {
                for (int32 keyIndex = 0; keyIndex < clip.m_RootDriver.PositionKeys.Num(); ++keyIndex)
                    clip.m_RootDriver.PositionKeys[keyIndex].Value *= rootDriverTranslationScale;
            }

            // Mapped track translations are in the mapped bone's parent-local space.
            // Convert them to skeleton root/object space so root-driver delta moves
            // along the same world-facing direction.
            const int32 mappedBoneIndex = mappedRootTrack.BoneIndex;
            if (mappedBoneIndex >= 0 && mappedBoneIndex < skeleton.m_Parent.Num())
            {
                const int32 mappedParentIndex = skeleton.m_Parent[mappedBoneIndex];
                if (mappedParentIndex >= 0 && mappedParentIndex < globalBindPose.Num())
                {
                    Vector3 parentBindTranslation(0.0f);
                    Quaternion parentBindRotation(1.0f, 0.0f, 0.0f, 0.0f);
                    Vector3 parentBindScale(1.0f);
                    DecomposeTRSForImport(
                        globalBindPose[mappedParentIndex],
                        parentBindTranslation,
                        parentBindRotation,
                        parentBindScale);

                    const Mat4 parentBindRotationM = FMath::mat4_cast(parentBindRotation);
                    for (int32 keyIndex = 0; keyIndex < clip.m_RootDriver.PositionKeys.Num(); ++keyIndex)
                    {
                        const Vector4 rotated =
                            parentBindRotationM * Vector4(clip.m_RootDriver.PositionKeys[keyIndex].Value, 0.0f);
                        clip.m_RootDriver.PositionKeys[keyIndex].Value = Vector3(rotated);
                    }

                    std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                              << "' converted mapped root driver '" << clip.m_RootDriver.NodeName.c_str()
                              << "' translation from parent-local to skeleton-root space using parent bone '"
                              << skeleton.m_BoneNames[mappedParentIndex].c_str() << "'.\n";
                }
            }

            // Prevent double translation (track translation + root driver delta).
            mappedRootTrack.PositionKeys.Clear();

            std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                      << "' applying mapped root driver track '" << clip.m_RootDriver.NodeName.c_str()
                      << "' and clearing its translation keys from bone track.\n";
        }
        else
        {
            std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                      << "' found no root driver channel.\n";
        }

        if (clip.m_RootDriver.bEnabled)
        {
            ANIM_LOG("=== ANIMATION ROOT DRIVER ===");
            ANIM_LOG("Track Bone: " << clip.m_RootDriver.NodeName.c_str());
            ANIM_LOG("PositionKeys: " << clip.m_RootDriver.PositionKeys.Num());
            ANIM_LOG("RotationKeys: " << clip.m_RootDriver.RotationKeys.Num());
            ANIM_LOG("ScaleKeys: " << clip.m_RootDriver.ScaleKeys.Num());
            if (!clip.m_RootDriver.PositionKeys.IsEmpty())
            {
                const AnimationVecKey& firstPos = clip.m_RootDriver.PositionKeys[0];
                ANIM_LOG("First Pos Key: (" << firstPos.Value.x << ", " << firstPos.Value.y << ", " << firstPos.Value.z << ")");
            }
            if (!clip.m_RootDriver.RotationKeys.IsEmpty())
            {
                const AnimationQuatKey& firstRot = clip.m_RootDriver.RotationKeys[0];
                ANIM_LOG("First Rot Key: (" << firstRot.Value.x << ", " << firstRot.Value.y << ", " << firstRot.Value.z << ", " << firstRot.Value.w << ")");
            }
            if (!clip.m_RootDriver.ScaleKeys.IsEmpty())
            {
                const AnimationVecKey& firstScale = clip.m_RootDriver.ScaleKeys[0];
                ANIM_LOG("First Scale Key: (" << firstScale.Value.x << ", " << firstScale.Value.y << ", " << firstScale.Value.z << ")");
            }
            ANIM_LOG_FLUSH();
        }

        if (skippedChannelCount > 0)
        {
            std::cout << "[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                      << "' mapped " << mappedChannelCount << "/" << srcAnim->mNumChannels
                      << " channels; skipped " << skippedChannelCount
                      << " channel(s) not in skeleton.";

            int32 printed = 0;
            const int32 maxPrinted = 8;
            for (const std::string& name : skippedChannelNames)
            {
                if (printed == 0)
                    std::cout << " Skipped names:";

                std::cout << " " << name;
                ++printed;
                if (printed >= maxPrinted)
                    break;
            }

            if (static_cast<int32>(skippedChannelNames.size()) > maxPrinted)
                std::cout << " ...";

            std::cout << "\n";
        }
        else
        {
            ANIM_LOG("[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                     << "' mapped " << mappedChannelCount << "/" << srcAnim->mNumChannels
                     << " channels; skipped 0 channel(s).");
        }

        if (skippedChannelCount > 0)
        {
            ANIM_LOG("[MeshLoader][Animation] Clip '" << clip.m_ClipName.c_str()
                     << "' mapped " << mappedChannelCount << "/" << srcAnim->mNumChannels
                     << " channels; skipped " << skippedChannelCount << " channel(s).");
        }
        ANIM_LOG_FLUSH();

        outAnimations.Add(std::move(clip));
    }

    std::cout << "[MeshLoader][Animation] Imported " << outAnimations.Num()
              << " clip(s) from " << path.c_str() << "\n";

    ANIM_LOG("[MeshLoader][Animation] Imported " << outAnimations.Num()
             << " clip(s) from " << path.c_str());
    ANIM_LOG_FLUSH();

    return !outAnimations.IsEmpty();
}




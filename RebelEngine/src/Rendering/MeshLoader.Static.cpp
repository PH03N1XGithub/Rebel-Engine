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

static std::string NormalizeImportPathUtf8(const String& path)
{
    return std::filesystem::u8path(path.c_str()).lexically_normal().string();
}

bool MeshLoader::LoadMeshFromFile(const String& path,
                                  TArray<Vertex>& outVertices,
                                  TArray<uint32>& outIndices)
{
    Assimp::Importer importer;
    const std::string normalizedPath = NormalizeImportPathUtf8(path);

    aiString ext;
    importer.GetExtensionList(ext);
    std::cout << "Assimp supports: " << ext.C_Str() << "\n";

    const aiScene* scene = importer.ReadFile(normalizedPath.c_str(),
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph
    );

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Assimp failed: " << importer.GetErrorString() << "\n";
        return false;
    }

    outVertices.Clear();
    outIndices.Clear();

    auto ToMat4 = [](const aiMatrix4x4& m) -> Mat4
    {
        Mat4 out;
        out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
        out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
        out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
        out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
        return out;
    };

    auto TryGetMetaNumber = [&](const char* key, double& outValue) -> bool
    {
        if (!scene->mMetaData || !key)
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
    };

    auto Dot = [](const Vector3& a, const Vector3& b) -> float
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    auto Cross = [](const Vector3& a, const Vector3& b) -> Vector3
    {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    };

    auto NormalizeSafe = [&](const Vector3& v, const Vector3& fallback) -> Vector3
    {
        const float lenSq = Dot(v, v);
        if (lenSq <= 1e-12f)
            return fallback;
        return FMath::normalize(v);
    };

    auto BuildOrthonormalAxisFromMatrix = [&](const Mat4& inM) -> Mat4
    {
        Vector3 axisX = NormalizeSafe(Vector3(inM[0]), Vector3(1.0f, 0.0f, 0.0f));
        Vector3 axisY = NormalizeSafe(Vector3(inM[1]), Vector3(0.0f, 1.0f, 0.0f));
        Vector3 axisZ = NormalizeSafe(Vector3(inM[2]), Vector3(0.0f, 0.0f, 1.0f));

        axisZ = NormalizeSafe(Cross(axisX, axisY), axisZ);
        axisY = NormalizeSafe(Cross(axisZ, axisX), axisY);

        if (Dot(Cross(axisX, axisY), axisZ) < 0.0f)
            axisZ *= -1.0f;

        Mat4 out(1.0f);
        out[0] = Vector4(axisX, 0.0f);
        out[1] = Vector4(axisY, 0.0f);
        out[2] = Vector4(axisZ, 0.0f);
        out[3] = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        return out;
    };

    Mat4 rootTransform(1.0f);
    if (scene->mRootNode)
        rootTransform = ToMat4(scene->mRootNode->mTransformation);

    auto ToSign = [](double v) -> int
    {
        return (v < 0.0) ? -1 : 1;
    };

    auto IsAxisIndex = [](int v) -> bool
    {
        return v >= 0 && v <= 2;
    };

    auto BuildAxisFromMetadata = [&](Mat4& outAxis) -> bool
    {
        double upAxisD = 0.0, upSignD = 1.0;
        double frontAxisD = 0.0, frontSignD = 1.0;
        double coordAxisD = 0.0, coordSignD = 1.0;

        if (!TryGetMetaNumber("UpAxis", upAxisD) ||
            !TryGetMetaNumber("UpAxisSign", upSignD) ||
            !TryGetMetaNumber("FrontAxis", frontAxisD) ||
            !TryGetMetaNumber("FrontAxisSign", frontSignD) ||
            !TryGetMetaNumber("CoordAxis", coordAxisD) ||
            !TryGetMetaNumber("CoordAxisSign", coordSignD))
        {
            return false;
        }

        const int upAxis = static_cast<int>(upAxisD);
        const int frontAxis = static_cast<int>(frontAxisD);
        const int coordAxis = static_cast<int>(coordAxisD);

        if (!IsAxisIndex(upAxis) || !IsAxisIndex(frontAxis) || !IsAxisIndex(coordAxis))
            return false;

        if (upAxis == frontAxis || upAxis == coordAxis || frontAxis == coordAxis)
            return false;

        const int upSign = ToSign(upSignD);
        const int frontSign = ToSign(frontSignD);
        const int coordSign = ToSign(coordSignD);

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
    };

    Mat4 axisOnly(1.0f);
    std::string axisSourceLabel = "metadata";
    const bool hasAxisMetadata = BuildAxisFromMetadata(axisOnly);

    if (!hasAxisMetadata)
    {
        axisSourceLabel = "root_fallback";
        axisOnly = BuildOrthonormalAxisFromMatrix(rootTransform);

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
            axisSourceLabel = "geometry_y_up_fallback";
        }
    }

    double unitScaleFactor = 0.0;
    bool hasUnitScale = TryGetMetaNumber("UnitScaleFactor", unitScaleFactor);
    if (!hasUnitScale)
        hasUnitScale = TryGetMetaNumber("OriginalUnitScaleFactor", unitScaleFactor);

    float metersPerUnit = 1.0f;
    if (hasUnitScale && unitScaleFactor > 0.0)
        metersPerUnit = static_cast<float>(unitScaleFactor * 0.01);

    Mat4 unitScaleMat(1.0f);
    unitScaleMat[0][0] = metersPerUnit;
    unitScaleMat[1][1] = metersPerUnit;
    unitScaleMat[2][2] = metersPerUnit;

    const Mat4 sourceToEngine = unitScaleMat * axisOnly;

    TArray<Mat4> meshNodeGlobal;
    meshNodeGlobal.Resize(scene->mNumMeshes);
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        meshNodeGlobal[m] = Mat4(1.0f);

    if (scene->mRootNode)
    {
        std::function<void(aiNode*, const Mat4&, bool)> GatherMeshNodeTransforms =
            [&](aiNode* node, const Mat4& parentGlobal, bool isRoot)
        {
            const Mat4 local = ToMat4(node->mTransformation);
            const Mat4 global = isRoot ? parentGlobal : (parentGlobal * local);

            for (unsigned i = 0; i < node->mNumMeshes; ++i)
            {
                const unsigned meshIndex = node->mMeshes[i];
                if (meshIndex < static_cast<unsigned>(meshNodeGlobal.Num()))
                    meshNodeGlobal[meshIndex] = global;
            }

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                GatherMeshNodeTransforms(node->mChildren[c], global, false);
        };

        GatherMeshNodeTransforms(scene->mRootNode, Mat4(1.0f), true);
    }

    TArray<Mat4> meshToEngine;
    meshToEngine.Resize(scene->mNumMeshes);

    TArray<Mat4> meshNormalAxis;
    meshNormalAxis.Resize(scene->mNumMeshes);

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const Mat4 meshNoScale = BuildOrthonormalAxisFromMatrix(meshNodeGlobal[m]);
        meshToEngine[m] = sourceToEngine * meshNodeGlobal[m];
        meshNormalAxis[m] = axisOnly * meshNoScale;
    }

    std::cout << "[MeshLoader][Static] axis source = " << axisSourceLabel
              << " | metersPerUnit = " << metersPerUnit << "\n";

    // Loop all meshes in the file
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        uint32_t baseVertex = static_cast<uint32_t>(outVertices.Num());
        const Mat4& meshXform = meshToEngine[m];
        const Mat4& meshNormalXform = meshNormalAxis[m];

        // ---- vertices ----
        for (unsigned i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex v{};
            v.Color = FMath::vec3(1.0f); // default white

            if (mesh->HasTextureCoords(0)) {
                aiVector3D uv = mesh->mTextureCoords[0][i];
                v.UV = FMath::vec2(uv.x, uv.y);
            } else {
                v.UV = FMath::vec2(0.0f);
            }

            {
                const Vector4 p = meshXform * Vector4(ToGlm(mesh->mVertices[i]), 1.0f);
                v.Position = Vector3(p);
            }

            if (mesh->HasNormals())
            {
                const Vector4 n = meshNormalXform * Vector4(ToGlm(mesh->mNormals[i]), 0.0f);
                v.Normal = FMath::normalize(Vector3(n));
            }

            /*if (mesh->HasNormals()) {
                FMath::vec3 n = ToGlm(mesh->mNormals[i]);
                v.Normal = n;
                v.Color  = 0.5f * (n + FMath::vec3(1.0f)); // map [-1,1] -> [0,1]
            }*/

            if (mesh->HasTextureCoords(0))
                v.UV = ToGlm2(mesh->mTextureCoords[0][i]);

            if (mesh->HasTangentsAndBitangents()) {
                const Vector4 t = meshNormalXform * Vector4(ToGlm(mesh->mTangents[i]), 0.0f);
                v.Tangent = Vector4(FMath::normalize(Vector3(t)), 1.0f);
            }

            if (mesh->HasVertexColors(0)) {
                aiColor4D c = mesh->mColors[0][i];
                v.Color = {c.r, c.g, c.b};
            }

            outVertices.Add(v);
        }

        // ---- indices (note the + baseVertex!) ----
        for (unsigned f = 0; f < mesh->mNumFaces; f++)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue; // should be tri after aiProcess_Triangulate

            outIndices.Add(baseVertex + face.mIndices[0]);
            outIndices.Add(baseVertex + face.mIndices[1]);
            outIndices.Add(baseVertex + face.mIndices[2]);
        }
    }

    std::cout << "Loaded mesh: " << path.c_str()
              << " | Vertices: " << outVertices.Num()
              << " | Indices: " << outIndices.Num() << "\n";

    return true;
}




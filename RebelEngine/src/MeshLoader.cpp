#include "EnginePch.h"
#include "MeshLoader.h"
#include "Animation/AnimationDiagnostics.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
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

bool MeshLoader::LoadMeshFromFile(const String& path,
                                  TArray<Vertex>& outVertices,
                                  TArray<uint32>& outIndices)
{
    Assimp::Importer importer;

    aiString ext;
    importer.GetExtensionList(ext);
    std::cout << "Assimp supports: " << ext.C_Str() << "\n";

    const aiScene* scene = importer.ReadFile(path.c_str(),
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

Mat4 ConvertMatrix(const aiMatrix4x4& m)
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


bool MeshLoader::LoadSkeletalMeshFromFile(
    const String& path,
    TArray<Vertex>& outVertices,
    TArray<uint32>& outIndices,
    SkeletonAsset& outSkeleton)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path.c_str(),
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights
    );

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "Assimp failed: " << importer.GetErrorString() << "\n";
        return false;
    }

    outVertices.Clear();
    outIndices.Clear();

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
        rootTransform = ConvertMatrix(scene->mRootNode->mTransformation);

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

    Vector3 geomExtent(0.0f, 0.0f, 0.0f);
    int dominantAxis = -1;

    if (!hasAxisMetadata)
    {
        axisSourceLabel = "root_fallback";
        axisOnly = BuildOrthonormalAxisFromMatrix(rootTransform);

        // Geometry heuristic fallback for files with missing axis metadata and identity root.
        // Many Mixamo-style files are Y-up and need +90deg around X to become Z-up.
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

        geomExtent = maxV - minV;

        if (geomExtent.x >= geomExtent.y && geomExtent.x >= geomExtent.z) dominantAxis = 0;
        else if (geomExtent.y >= geomExtent.x && geomExtent.y >= geomExtent.z) dominantAxis = 1;
        else dominantAxis = 2;

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

        // If Y is dominant OR XY are close (typical T-pose: arms can make X slightly larger),
        // and Z is clearly thinner, treat source as Y-up and rotate to Z-up.
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

    // Build per-mesh node transforms (excluding root transform to avoid double-applying root axis fallback).
    TArray<Mat4> meshNodeGlobal;
    meshNodeGlobal.Resize(scene->mNumMeshes);
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
        meshNodeGlobal[m] = Mat4(1.0f);

    if (scene->mRootNode)
    {
        std::function<void(aiNode*, const Mat4&, bool)> GatherMeshNodeTransforms =
            [&](aiNode* node, const Mat4& parentGlobal, bool isRoot)
        {
            const Mat4 local = ConvertMatrix(node->mTransformation);
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

    auto PrintMat = [&](const char* label, const Mat4& m)
    {
        std::cout << "[MeshLoader][Debug] " << label << "\n";
        for (int r = 0; r < 4; ++r)
        {
            std::cout << "  ["
                      << m[0][r] << ", "
                      << m[1][r] << ", "
                      << m[2][r] << ", "
                      << m[3][r] << "]\n";
        }
    };

    auto LogMeta = [&](const char* key)
    {
        double v = 0.0;
        const bool ok = TryGetMetaNumber(key, v);
        if (ok)
            std::cout << "[MeshLoader][Debug] meta " << key << " = " << v << "\n";
        else
            std::cout << "[MeshLoader][Debug] meta " << key << " = <missing>\n";
    };

    std::cout << "[MeshLoader][Debug] Import skeletal: " << path.c_str() << "\n";
    LogMeta("UpAxis");
    LogMeta("UpAxisSign");
    LogMeta("FrontAxis");
    LogMeta("FrontAxisSign");
    LogMeta("CoordAxis");
    LogMeta("CoordAxisSign");
    LogMeta("UnitScaleFactor");
    LogMeta("OriginalUnitScaleFactor");
    std::cout << "[MeshLoader][Debug] axis source = " << axisSourceLabel << "\n";
    std::cout << "[MeshLoader][Debug] geom extent = [" << geomExtent.x << ", " << geomExtent.y << ", " << geomExtent.z << "]\n";
    std::cout << "[MeshLoader][Debug] dominant axis = " << dominantAxis << "\n";
    if (!hasAxisMetadata)
    {
        const float xyMinDbg = std::min(geomExtent.x, geomExtent.y);
        const float xyMaxDbg = std::max(geomExtent.x, geomExtent.y);
        const bool xyCloseDbg = (xyMaxDbg / std::max(1e-4f, xyMinDbg)) < 1.25f;
        const bool zIsThinDbg = geomExtent.z * 2.0f < xyMinDbg;
        std::cout << "[MeshLoader][Debug] heuristic xyClose=" << (xyCloseDbg ? 1 : 0)
                  << " zIsThin=" << (zIsThinDbg ? 1 : 0) << "\n";
    }
    std::cout << "[MeshLoader][Debug] metersPerUnit = " << metersPerUnit << "\n";
    PrintMat("RootTransform", rootTransform);
    PrintMat("AxisOnly", axisOnly);
    PrintMat("SourceToEngine", sourceToEngine);

    const unsigned debugMeshCount = (scene->mNumMeshes < 3u) ? scene->mNumMeshes : 3u;
    for (unsigned m = 0; m < debugMeshCount; ++m)
    {
        std::cout << "[MeshLoader][Debug] mesh " << m << "\n";
        PrintMat("MeshNodeGlobal", meshNodeGlobal[m]);
        PrintMat("MeshToEngine", meshToEngine[m]);
    }

    {
        Vector3 rootTranslation(0.0f);
        Quaternion rootRotation(1.0f, 0.0f, 0.0f, 0.0f);
        Vector3 rootScale(1.0f);
        DecomposeTRSForImport(rootTransform, rootTranslation, rootRotation, rootScale);

        ANIM_LOG("=== FBX GLOBAL SETTINGS ===");
        ANIM_LOG("Context: SkeletalMesh Import");
        ANIM_LOG("Source: " << path.c_str());
        ANIM_LOG("UnitScaleFactorRaw: " << (hasUnitScale ? unitScaleFactor : -1.0));
        ANIM_LOG("MetersPerUnit: " << metersPerUnit);
        ANIM_LOG("AxisSource: " << axisSourceLabel);
        ANIM_LOG("SceneScale(approx): " << metersPerUnit);
        ANIM_LOG("RootNode Scale: (" << rootScale.x << ", " << rootScale.y << ", " << rootScale.z << ")");
        ANIM_LOG("RootNode Translation: (" << rootTranslation.x << ", " << rootTranslation.y << ", " << rootTranslation.z << ")");
        ANIM_LOG("RootNode Transform:");
        LogMatrixForDiagnostics(rootTransform);
        ANIM_LOG("AxisOnly:");
        LogMatrixForDiagnostics(axisOnly);
        ANIM_LOG("SourceToEngine:");
        LogMatrixForDiagnostics(sourceToEngine);

        double upAxisMeta = 0.0;
        double upSignMeta = 0.0;
        double frontAxisMeta = 0.0;
        double frontSignMeta = 0.0;
        double coordAxisMeta = 0.0;
        double coordSignMeta = 0.0;
        ANIM_LOG("UpAxis: " << (TryGetMetaNumber("UpAxis", upAxisMeta) ? upAxisMeta : -1.0));
        ANIM_LOG("UpAxisSign: " << (TryGetMetaNumber("UpAxisSign", upSignMeta) ? upSignMeta : 0.0));
        ANIM_LOG("FrontAxis: " << (TryGetMetaNumber("FrontAxis", frontAxisMeta) ? frontAxisMeta : -1.0));
        ANIM_LOG("FrontAxisSign: " << (TryGetMetaNumber("FrontAxisSign", frontSignMeta) ? frontSignMeta : 0.0));
        ANIM_LOG("CoordAxis: " << (TryGetMetaNumber("CoordAxis", coordAxisMeta) ? coordAxisMeta : -1.0));
        ANIM_LOG("CoordAxisSign: " << (TryGetMetaNumber("CoordAxisSign", coordSignMeta) ? coordSignMeta : 0.0));
        ANIM_LOG_FLUSH();
    }

    // --------------------------------------------------
    // Build deterministic skeleton from deform bones + animation channels + ancestors.
    // --------------------------------------------------

    std::unordered_map<std::string, const aiNode*> nodeByName;
    std::unordered_map<std::string, std::string> nodeParentByName;
    std::unordered_map<std::string, Mat4> nodeGlobalByName;
    if (scene->mRootNode)
    {
        std::function<void(aiNode*, const std::string&, const Mat4&, bool)> gatherNodes =
            [&](aiNode* node, const std::string& parentName, const Mat4& parentGlobal, bool isRoot)
        {
            if (!node)
                return;

            const std::string nodeName = node->mName.C_Str();
            const Mat4 local = ConvertMatrix(node->mTransformation);
            const Mat4 global = isRoot ? parentGlobal : (parentGlobal * local);

            nodeByName[nodeName] = node;
            nodeGlobalByName[nodeName] = global;
            if (!parentName.empty())
                nodeParentByName[nodeName] = parentName;

            for (unsigned c = 0; c < node->mNumChildren; ++c)
                gatherNodes(node->mChildren[c], nodeName, global, false);
        };

        gatherNodes(scene->mRootNode, std::string(), Mat4(1.0f), true);
    }

    std::unordered_set<std::string> deformBoneNames;
    std::unordered_set<std::string> animationChannelNames;
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned b = 0; b < mesh->mNumBones; ++b)
            deformBoneNames.insert(mesh->mBones[b]->mName.C_Str());
    }

    for (unsigned a = 0; a < scene->mNumAnimations; ++a)
    {
        const aiAnimation* anim = scene->mAnimations[a];
        if (!anim)
            continue;

        for (unsigned c = 0; c < anim->mNumChannels; ++c)
        {
            const aiNodeAnim* channel = anim->mChannels[c];
            if (!channel)
                continue;
            animationChannelNames.insert(channel->mNodeName.C_Str());
        }
    }

    std::unordered_set<std::string> requiredBoneNames = deformBoneNames;
    requiredBoneNames.insert(animationChannelNames.begin(), animationChannelNames.end());

    std::unordered_set<std::string> missingSceneNodes;
    std::vector<std::string> requiredSnapshot(requiredBoneNames.begin(), requiredBoneNames.end());
    for (const std::string& name : requiredSnapshot)
    {
        auto nodeIt = nodeByName.find(name);
        if (nodeIt == nodeByName.end())
        {
            missingSceneNodes.insert(name);
            continue;
        }

        const aiNode* cursor = nodeIt->second->mParent;
        while (cursor)
        {
            requiredBoneNames.insert(cursor->mName.C_Str());
            cursor = cursor->mParent;
        }
    }

    std::vector<std::string> orderedBoneNames;
    orderedBoneNames.reserve(requiredBoneNames.size());
    std::unordered_set<std::string> visitedRequired;

    std::function<void(aiNode*)> appendRequiredNodes = [&](aiNode* node)
    {
        if (!node)
            return;

        const std::string nodeName = node->mName.C_Str();
        if (requiredBoneNames.find(nodeName) != requiredBoneNames.end())
        {
            orderedBoneNames.push_back(nodeName);
            visitedRequired.insert(nodeName);
        }

        for (unsigned c = 0; c < node->mNumChildren; ++c)
            appendRequiredNodes(node->mChildren[c]);
    };

    appendRequiredNodes(scene->mRootNode);

    std::vector<std::string> unvisitedRequired;
    unvisitedRequired.reserve(requiredBoneNames.size());
    for (const std::string& name : requiredBoneNames)
    {
        if (visitedRequired.find(name) == visitedRequired.end())
            unvisitedRequired.push_back(name);
    }
    std::sort(unvisitedRequired.begin(), unvisitedRequired.end());
    orderedBoneNames.insert(orderedBoneNames.end(), unvisitedRequired.begin(), unvisitedRequired.end());

    std::unordered_map<std::string, uint32> boneMap;
    boneMap.reserve(orderedBoneNames.size());
    for (uint32 i = 0; i < static_cast<uint32>(orderedBoneNames.size()); ++i)
        boneMap[orderedBoneNames[i]] = i;

    const uint32 boneCounter = static_cast<uint32>(orderedBoneNames.size());
    if (boneCounter == 0)
    {
        std::cerr << "No skeleton nodes found in skeletal mesh.\n";
        return false;
    }

    if (boneCounter > 255)
    {
        std::cerr << "Skeletal mesh has " << boneCounter
                  << " skeleton nodes, but current vertex format supports max 255 bone indices.\n";
        return false;
    }

    outSkeleton.m_Parent.Resize(boneCounter);
    outSkeleton.m_InvBind.Resize(boneCounter);
    outSkeleton.m_BoneNames.Resize(boneCounter);

    for (uint32 i = 0; i < boneCounter; ++i)
    {
        outSkeleton.m_Parent[i] = -1;
        outSkeleton.m_InvBind[i] = Mat4(1.0f);
        outSkeleton.m_BoneNames[i] = String(orderedBoneNames[i].c_str());
    }

    for (uint32 i = 0; i < boneCounter; ++i)
    {
        const std::string boneName = orderedBoneNames[i];
        const auto nodeIt = nodeByName.find(boneName);
        if (nodeIt == nodeByName.end())
            continue;

        const aiNode* cursor = nodeIt->second->mParent;
        int32 parentIndex = -1;
        while (cursor)
        {
            const auto parentIt = boneMap.find(cursor->mName.C_Str());
            if (parentIt != boneMap.end())
            {
                parentIndex = static_cast<int32>(parentIt->second);
                break;
            }
            cursor = cursor->mParent;
        }

        outSkeleton.m_Parent[i] = parentIndex;
    }

    struct InvBindAssignment
    {
        bool bAssigned = false;
        Mat4 Value = Mat4(1.0f);
        uint32 SourceMeshIndex = 0;
    };
    std::vector<InvBindAssignment> invBindAssignments(static_cast<size_t>(boneCounter));

    auto MaxAbsMatrixDiff = [](const Mat4& a, const Mat4& b) -> float
    {
        float maxDiff = 0.0f;
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
                maxDiff = FMath::max(maxDiff, std::fabs(a[c][r] - b[c][r]));
        }
        return maxDiff;
    };

    constexpr float kInvBindMismatchEpsilon = 1e-4f;

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        const Mat4 meshToEngineInv = FMath::inverse(meshToEngine[m]);

        for (unsigned b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            const std::string name = bone->mName.C_Str();
            const auto mapIt = boneMap.find(name);
            if (mapIt == boneMap.end())
                continue;

            const uint32 index = mapIt->second;
            const Mat4 srcInvBind = ConvertMatrix(bone->mOffsetMatrix);
            const Mat4 candidateInvBind = srcInvBind * meshToEngineInv;

            InvBindAssignment& slot = invBindAssignments[index];
            if (!slot.bAssigned)
            {
                slot.bAssigned = true;
                slot.Value = candidateInvBind;
                slot.SourceMeshIndex = m;
            }
            else
            {
                const float diff = MaxAbsMatrixDiff(slot.Value, candidateInvBind);
                if (diff > kInvBindMismatchEpsilon)
                {
                    std::cerr << "[MeshLoader][Skeleton] WARNING bone '" << name
                              << "' inverse bind mismatch between mesh " << slot.SourceMeshIndex
                              << " and mesh " << m << " (maxDiff=" << diff
                              << "). Keeping first value.\n";
                    if (diff > 1e-3f)
                    {
                        ANIM_LOG("WARNING: inverseBind mismatch > 0.001 for bone '" << name
                                 << "' maxDiff=" << diff
                                 << " (mesh " << slot.SourceMeshIndex << " vs " << m << ")");
                    }
                }
            }
        }
    }

    for (uint32 i = 0; i < boneCounter; ++i)
    {
        InvBindAssignment& slot = invBindAssignments[i];
        if (slot.bAssigned)
        {
            outSkeleton.m_InvBind[i] = slot.Value;
            continue;
        }

        const std::string boneName = orderedBoneNames[i];
        const auto nodeGlobalIt = nodeGlobalByName.find(boneName);
        if (nodeGlobalIt != nodeGlobalByName.end())
        {
            const Mat4 globalBind = sourceToEngine * nodeGlobalIt->second;
            outSkeleton.m_InvBind[i] = FMath::inverse(globalBind);
        }
        else
        {
            outSkeleton.m_InvBind[i] = Mat4(1.0f);
            std::cerr << "[MeshLoader][Skeleton] WARNING bone '" << boneName
                      << "' has no bind transform source. Using identity inverse bind.\n";
        }
    }

    int32 rootBoneCount = 0;
    for (uint32 i = 0; i < boneCounter; ++i)
    {
        const int32 parent = outSkeleton.m_Parent[i];
        if (parent < 0)
        {
            ++rootBoneCount;
            continue;
        }

        if (parent >= static_cast<int32>(boneCounter))
        {
            std::cerr << "[MeshLoader][Skeleton] WARNING bone '" << outSkeleton.m_BoneNames[i].c_str()
                      << "' has invalid parent index " << parent << ". Resetting to root.\n";
            outSkeleton.m_Parent[i] = -1;
            ++rootBoneCount;
        }
    }

    std::vector<uint8> visitState(static_cast<size_t>(boneCounter), 0);
    std::function<void(uint32)> ensureAcyclic = [&](uint32 boneIndex)
    {
        if (visitState[boneIndex] == 2)
            return;

        if (visitState[boneIndex] == 1)
        {
            std::cerr << "[MeshLoader][Skeleton] WARNING cycle detected at bone '"
                      << outSkeleton.m_BoneNames[boneIndex].c_str()
                      << "'. Resetting parent to root.\n";
            outSkeleton.m_Parent[boneIndex] = -1;
            visitState[boneIndex] = 2;
            return;
        }

        visitState[boneIndex] = 1;
        const int32 parent = outSkeleton.m_Parent[boneIndex];
        if (parent >= 0 && parent < static_cast<int32>(boneCounter))
            ensureAcyclic(static_cast<uint32>(parent));
        visitState[boneIndex] = 2;
    };

    for (uint32 i = 0; i < boneCounter; ++i)
        ensureAcyclic(i);

    rootBoneCount = 0;
    for (uint32 i = 0; i < boneCounter; ++i)
    {
        if (outSkeleton.m_Parent[i] < 0)
            ++rootBoneCount;
    }

    std::cout << "[MeshLoader][Skeleton] Bones=" << boneCounter
              << " deform=" << deformBoneNames.size()
              << " animChannels=" << animationChannelNames.size()
              << " roots=" << rootBoneCount << "\n";

    if (!missingSceneNodes.empty())
    {
        int32 printed = 0;
        const int32 maxPrinted = 8;
        std::cout << "[MeshLoader][Skeleton] WARNING missing " << missingSceneNodes.size()
                  << " required node(s) in scene hierarchy:";
        for (const std::string& name : missingSceneNodes)
        {
            std::cout << " " << name;
            if (++printed >= maxPrinted)
                break;
        }
        if (static_cast<int32>(missingSceneNodes.size()) > maxPrinted)
            std::cout << " ...";
        std::cout << "\n";
    }

    {
        ANIM_LOG("=== SKELETON ===");
        ANIM_LOG("BoneCount: " << boneCounter);

        TArray<Mat4> globalBindPose;
        TArray<Mat4> localBindPose;
        globalBindPose.Resize(boneCounter);
        localBindPose.Resize(boneCounter);

        for (uint32 i = 0; i < boneCounter; ++i)
            globalBindPose[i] = FMath::inverse(outSkeleton.m_InvBind[i]);

        for (uint32 i = 0; i < boneCounter; ++i)
        {
            const int32 parentIndex = outSkeleton.m_Parent[i];
            if (parentIndex >= 0 && parentIndex < static_cast<int32>(boneCounter))
                localBindPose[i] = FMath::inverse(globalBindPose[parentIndex]) * globalBindPose[i];
            else
                localBindPose[i] = globalBindPose[i];
        }

        for (uint32 i = 0; i < boneCounter; ++i)
        {
            const int32 parentIndex = outSkeleton.m_Parent[i];
            const char* parentName = "<ROOT>";
            if (parentIndex >= 0 && parentIndex < outSkeleton.m_BoneNames.Num())
                parentName = outSkeleton.m_BoneNames[parentIndex].c_str();

            Vector3 bindGlobalT(0.0f);
            Quaternion bindGlobalR(1.0f, 0.0f, 0.0f, 0.0f);
            Vector3 bindGlobalS(1.0f);
            DecomposeTRSForImport(globalBindPose[i], bindGlobalT, bindGlobalR, bindGlobalS);

            Vector3 bindLocalT(0.0f);
            Quaternion bindLocalR(1.0f, 0.0f, 0.0f, 0.0f);
            Vector3 bindLocalS(1.0f);
            DecomposeTRSForImport(localBindPose[i], bindLocalT, bindLocalR, bindLocalS);

            const Mat4 bindIdentity = globalBindPose[i] * outSkeleton.m_InvBind[i];
            const float bindError = MatrixMaxAbsIdentityErrorForDiagnostics(bindIdentity);

            ANIM_LOG("[Bone " << i << "]");
            ANIM_LOG("Name: " << outSkeleton.m_BoneNames[i].c_str());
            ANIM_LOG("Parent: " << parentName << " (" << parentIndex << ")");
            ANIM_LOG("Bind Global Translation: (" << bindGlobalT.x << ", " << bindGlobalT.y << ", " << bindGlobalT.z << ")");
            ANIM_LOG("Bind Local Translation: (" << bindLocalT.x << ", " << bindLocalT.y << ", " << bindLocalT.z << ")");
            ANIM_LOG("globalBind * inverseBind error: " << bindError);

            if (bindError > 1e-3f)
            {
                ANIM_LOG("WARNING: inverseBind mismatch > 0.001 for bone '" << outSkeleton.m_BoneNames[i].c_str()
                         << "' error=" << bindError);
            }
        }

        ANIM_LOG_FLUSH();
    }

    // --------------------------------------------------
    // Load geometry
    // --------------------------------------------------

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        const Mat4& meshXform = meshToEngine[m];
        const Mat4& meshNormalXform = meshNormalAxis[m];

        uint32 baseVertex = static_cast<uint32>(outVertices.Num());

        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
        {
            Vertex v{};
            v.Color = FMath::vec3(1.0f);

            // ---- Position (axis + unit + mesh-node transform) ----
            {
                const Vector4 p = meshXform * Vector4(ToGlm(mesh->mVertices[i]), 1.0f);
                v.Position = Vector3(p);
            }

            // ---- Normal (axis only, no scale) ----
            if (mesh->HasNormals())
            {
                const Vector4 n = meshNormalXform * Vector4(ToGlm(mesh->mNormals[i]), 0.0f);
                v.Normal = FMath::normalize(Vector3(n));
            }

            if (mesh->HasTextureCoords(0))
                v.UV = ToGlm2(mesh->mTextureCoords[0][i]);

            if (mesh->HasTangentsAndBitangents())
            {
                const Vector4 t = meshNormalXform * Vector4(ToGlm(mesh->mTangents[i]), 0.0f);
                v.Tangent = Vector4(FMath::normalize(Vector3(t)), 1.0f);
            }

            for (int k = 0; k < 4; ++k)
            {
                v.BoneIndex[k] = 0;
                v.BoneWeight[k] = 0.0f;
            }

            outVertices.Add(v);
        }

        // ---- Indices ----
        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;

            outIndices.Add(baseVertex + face.mIndices[0]);
            outIndices.Add(baseVertex + face.mIndices[1]);
            outIndices.Add(baseVertex + face.mIndices[2]);
        }

        // --------------------------------------------------
        // Assign bone weights
        // --------------------------------------------------

        for (unsigned b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            std::string name = bone->mName.C_Str();

            uint32 boneIndex = boneMap[name];

            for (unsigned w = 0; w < bone->mNumWeights; ++w)
            {
                uint32 vertexID = baseVertex + bone->mWeights[w].mVertexId;
                float weight = bone->mWeights[w].mWeight;

                Vertex& v = outVertices[vertexID];

                int slot = -1;
                for (int i = 0; i < 4; ++i)
                {
                    if (v.BoneWeight[i] == 0.0f)
                    {
                        slot = i;
                        break;
                    }
                }

                if (slot == -1)
                {
                    int smallest = 0;
                    for (int i = 1; i < 4; ++i)
                        if (v.BoneWeight[i] < v.BoneWeight[smallest])
                            smallest = i;

                    if (weight > v.BoneWeight[smallest])
                        slot = smallest;
                }

                if (slot != -1)
                {
                    v.BoneIndex[slot] = static_cast<uint8>(boneIndex);
                    v.BoneWeight[slot] = weight;
                }
            }
        }
    }

    // ---- Normalize weights ----
    uint32 zeroWeightVertexCount = 0;
    uint32 invalidBoneIndexInfluenceCount = 0;
    for (uint32 i = 0; i < outVertices.Num(); ++i)
    {
        Vertex& v = outVertices[i];

        float sum = 0.0f;
        for (int k = 0; k < 4; ++k)
            sum += v.BoneWeight[k];

        if (sum > 0.0f)
        {
            for (int k = 0; k < 4; ++k)
                v.BoneWeight[k] /= sum;
        }
        else
        {
            ++zeroWeightVertexCount;
        }

        for (int k = 0; k < 4; ++k)
        {
            if (v.BoneWeight[k] <= 0.0f)
                continue;

            if (v.BoneIndex[k] >= boneCounter)
            {
                ++invalidBoneIndexInfluenceCount;
                ANIM_LOG("WARNING: bone index out of range at vertex " << i
                         << " influenceSlot=" << k
                         << " boneIndex=" << static_cast<uint32>(v.BoneIndex[k])
                         << " boneCount=" << boneCounter);
            }
        }
    }
    if (zeroWeightVertexCount > 0)
    {
        std::cerr << "[Skinning][Weights] WARNING " << zeroWeightVertexCount
                  << " vertices have zero total bone weight in " << path.c_str()
                  << " (shader should fallback to identity skinning for these vertices).\n";
    }

    ANIM_LOG("=== MESH SKIN WEIGHTS ===");
    const int32 vertexPreviewCount = static_cast<int32>(std::min<uint32>(20u, outVertices.Num()));
    for (int32 i = 0; i < vertexPreviewCount; ++i)
    {
        const Vertex& v = outVertices[i];
        const float weightSum = v.BoneWeight[0] + v.BoneWeight[1] + v.BoneWeight[2] + v.BoneWeight[3];
        ANIM_LOG("Vertex " << i << ":");
        ANIM_LOG("Indices: (" << static_cast<uint32>(v.BoneIndex[0]) << ", "
                << static_cast<uint32>(v.BoneIndex[1]) << ", "
                << static_cast<uint32>(v.BoneIndex[2]) << ", "
                << static_cast<uint32>(v.BoneIndex[3]) << ")");
        ANIM_LOG("Weights: (" << v.BoneWeight[0] << ", "
                << v.BoneWeight[1] << ", "
                << v.BoneWeight[2] << ", "
                << v.BoneWeight[3] << ")");
        ANIM_LOG("WeightSum: " << weightSum);
        if (weightSum == 0.0f)
            ANIM_LOG("WARNING: vertex weight sum == 0 at vertex " << i);
    }

    ANIM_LOG("Total vertices with zero weights: " << zeroWeightVertexCount);
    ANIM_LOG("Total vertices with invalid indices: " << invalidBoneIndexInfluenceCount);
    if (zeroWeightVertexCount > 0)
        ANIM_LOG("WARNING: vertex weight sum == 0 detected on " << zeroWeightVertexCount << " vertices.");
    if (invalidBoneIndexInfluenceCount > 0)
        ANIM_LOG("WARNING: bone index out of range count = " << invalidBoneIndexInfluenceCount);
    ANIM_LOG_FLUSH();

    std::cout << "Loaded skeletal mesh: " << path.c_str()
              << " | Bones: " << boneCounter
              << " | Vertices: " << outVertices.Num()
              << " | Indices: " << outIndices.Num() << "\n";

    return true;
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
    const aiScene* scene = importer.ReadFile(path.c_str(),
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
            const auto boneIt = boneNameToIndex.find(boneName);
            if (boneIt == boneNameToIndex.end())
            {
                ++skippedChannelCount;
                skippedChannelNames.insert(boneName);
                unmappedChannels.push_back({boneName, channel});
                continue;
            }

            ++mappedChannelCount;
            mappedChannelNames.insert(boneName);

            AnimationTrack track;
            track.BoneName = String(boneName.c_str());
            track.BoneIndex = boneIt->second;
            FillTrackFromChannel(track, channel);

            // Bake skipped non-bone parent transforms into this track so channel local
            // space matches our collapsed skeleton hierarchy.
            const int32 trackBoneIndex = track.BoneIndex;
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

                        for (int32 keyIndex = 0; keyIndex < track.PositionKeys.Num(); ++keyIndex)
                        {
                            const Vector4 transformed =
                                engineBridgeNoScale * Vector4(track.PositionKeys[keyIndex].Value, 1.0f);
                            track.PositionKeys[keyIndex].Value = Vector3(transformed);
                        }

                        for (int32 keyIndex = 0; keyIndex < track.RotationKeys.Num(); ++keyIndex)
                        {
                            const Quaternion localQ = FMath::normalize(track.RotationKeys[keyIndex].Value);
                            track.RotationKeys[keyIndex].Value = FMath::normalize(bridgeRotation * localQ);
                        }

                        std::cout << "[MeshLoader][Animation] Track '" << track.BoneName.c_str()
                                  << "' baked " << bridgeNodes.size()
                                  << " skipped parent transform(s) into channel data.\n";

                        if (hasBridgeScale)
                        {
                            std::cout << "[MeshLoader][Animation] Track '" << track.BoneName.c_str()
                                      << "' ignored skipped-parent scale during bake ("
                                      << sourceBridgeScale.x << ", " << sourceBridgeScale.y << ", " << sourceBridgeScale.z
                                      << ").\n";
                        }
                    }
                    else if (!reachedCollapsedParent && !mappedParentName.empty())
                    {
                        std::cerr << "[MeshLoader][Animation] Track '" << track.BoneName.c_str()
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

                for (int32 keyIndex = 0; keyIndex < track.PositionKeys.Num(); ++keyIndex)
                    track.PositionKeys[keyIndex].Value = bindTranslation + track.PositionKeys[keyIndex].Value;

                for (int32 keyIndex = 0; keyIndex < track.RotationKeys.Num(); ++keyIndex)
                {
                    const Quaternion relQ = FMath::normalize(track.RotationKeys[keyIndex].Value);
                    track.RotationKeys[keyIndex].Value = FMath::normalize(bindRotation * relQ);
                }

                for (int32 keyIndex = 0; keyIndex < track.ScaleKeys.Num(); ++keyIndex)
                    track.ScaleKeys[keyIndex].Value = bindScale * track.ScaleKeys[keyIndex].Value;

                std::cout << "[MeshLoader][Animation] Track '" << track.BoneName.c_str()
                          << "' converted from explicit relative mode to absolute local keys.\n";
            }

            ANIM_LOG("=== ANIMATION TRACK ===");
            ANIM_LOG("Track Bone: " << track.BoneName.c_str());
            ANIM_LOG("BoneIndex: " << track.BoneIndex);
            ANIM_LOG("PositionKeys: " << track.PositionKeys.Num());
            ANIM_LOG("RotationKeys: " << track.RotationKeys.Num());
            ANIM_LOG("ScaleKeys: " << track.ScaleKeys.Num());

            if (!track.PositionKeys.IsEmpty())
            {
                const AnimationVecKey& firstPos = track.PositionKeys[0];
                ANIM_LOG("First Pos Key: (" << firstPos.Value.x << ", " << firstPos.Value.y << ", " << firstPos.Value.z << ")");
            }
            if (!track.RotationKeys.IsEmpty())
            {
                const AnimationQuatKey& firstRot = track.RotationKeys[0];
                ANIM_LOG("First Rot Key: (" << firstRot.Value.x << ", " << firstRot.Value.y << ", " << firstRot.Value.z << ", " << firstRot.Value.w << ")");
            }
            if (!track.ScaleKeys.IsEmpty())
            {
                const AnimationVecKey& firstScale = track.ScaleKeys[0];
                ANIM_LOG("First Scale Key: (" << firstScale.Value.x << ", " << firstScale.Value.y << ", " << firstScale.Value.z << ")");
            }

            if (track.BoneIndex >= 0 && track.BoneIndex < localBindPose.Num() && !track.PositionKeys.IsEmpty())
            {
                Vector3 bindTranslation(0.0f);
                Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
                Vector3 bindScale(1.0f);
                DecomposeTRSForImport(localBindPose[track.BoneIndex], bindTranslation, bindRotation, bindScale);

                const float bindLength = FMath::length(bindTranslation);
                const float animLength = FMath::length(track.PositionKeys[0].Value);

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
                             << track.BoneName.c_str() << "' ratio=" << scaleRatio);
                }
            }
            ANIM_LOG_FLUSH();

            clip.m_Tracks.Add(std::move(track));
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

        auto IsPreferredRootName = [](const std::string& nameLower) -> bool
        {
            // Use substring matching to handle names like "Armature|Root" or "mixamorig:Hips".
            static const char* kPreferredTokens[] = {
                "armature", "root", "hips", "pelvis"
            };

            for (const char* token : kPreferredTokens)
            {
                if (nameLower.find(token) != std::string::npos)
                    return true;
            }
            return false;
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

        // Fallback: if all channels are mapped into bones, pick a mapped root-like track.
        // This handles clips where root motion is authored on a mapped root bone.
        if (!rootDriverChannel)
        {
            int32 bestPreferredDepth = INT_MAX;
            for (int32 i = 0; i < clip.m_Tracks.Num(); ++i)
            {
                const AnimationTrack& track = clip.m_Tracks[i];
                const std::string nameLower = ToLowerCopy(track.BoneName.c_str());
                if (!IsPreferredRootName(nameLower))
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
                int32 bestDepth = INT_MAX;
                for (int32 i = 0; i < clip.m_Tracks.Num(); ++i)
                {
                    const AnimationTrack& track = clip.m_Tracks[i];
                    if (!TrackHasTranslationMotion(track))
                        continue;

                    const int32 depth = ComputeBoneDepth(track.BoneIndex);
                    if (depth < bestDepth)
                    {
                        bestDepth = depth;
                        mappedRootDriverTrackIndex = i;
                    }
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

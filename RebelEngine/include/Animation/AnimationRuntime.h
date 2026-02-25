#pragma once

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <functional>
#include <vector>

#include "Animation/AnimationAsset.h"
#include "Animation/SkeletonAsset.h"

namespace AnimationRuntime
{
inline Vector3 ExtractTranslation(const Mat4& m)
{
    return Vector3(m[3]);
}

inline Mat4 ComposeTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
{
    return FMath::translate(Mat4(1.0f), translation) * FMath::mat4_cast(rotation) * FMath::scale(Mat4(1.0f), scale);
}

inline void DecomposeTRS(const Mat4& m, Vector3& outTranslation, Quaternion& outRotation, Vector3& outScale)
{
    outTranslation = ExtractTranslation(m);

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

inline float NormalizePlaybackTime(float timeSeconds, float durationSeconds, bool bLooping)
{
    if (durationSeconds <= 1e-6f)
        return 0.0f;

    if (bLooping)
    {
        float wrapped = std::fmod(timeSeconds, durationSeconds);
        if (wrapped < 0.0f)
            wrapped += durationSeconds;
        return wrapped;
    }

    return FMath::clamp(timeSeconds, 0.0f, durationSeconds);
}

template<typename TKey>
inline int32 FindKeySpanStart(const TArray<TKey>& keys, float sampleTime)
{
    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 1)
        return 0;

    if (sampleTime <= keys[0].TimeSeconds)
        return 0;

    auto begin = keys.begin();
    auto end = keys.end();
    auto upper = std::lower_bound(
        begin + 1,
        end,
        sampleTime,
        [](const TKey& key, float time)
        {
            return key.TimeSeconds < time;
        });

    if (upper == end)
        return keyCount - 1;

    const int32 upperIndex = static_cast<int32>(upper - begin);
    return upperIndex - 1;
}

inline Vector3 SampleVectorKeys(const TArray<AnimationVecKey>& keys, float sampleTime, const Vector3& fallback)
{
    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount == 0)
        return fallback;

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
        return keys[0].Value;

    const int32 fromIndex = FindKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
        return keys[keyCount - 1].Value;

    const AnimationVecKey& a = keys[fromIndex];
    const AnimationVecKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
        return b.Value;

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    return FMath::mix(a.Value, b.Value, alpha);
}

inline Quaternion SampleRotationKeys(const TArray<AnimationQuatKey>& keys, float sampleTime, const Quaternion& fallback)
{
    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount == 0)
        return fallback;

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
        return FMath::normalize(keys[0].Value);

    const int32 fromIndex = FindKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
        return FMath::normalize(keys[keyCount - 1].Value);

    const AnimationQuatKey& a = keys[fromIndex];
    const AnimationQuatKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
        return FMath::normalize(b.Value);

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    Quaternion qb = b.Value;
    if (FMath::dot(a.Value, qb) < 0.0f)
        qb = -qb;

    return FMath::normalize(FMath::slerp(a.Value, qb, alpha));
}

inline bool BuildBindPoses(const SkeletonAsset* skeleton, TArray<Mat4>& outLocalBind, TArray<Mat4>& outGlobalBind)
{
    if (!skeleton)
        return false;

    const int32 boneCount = static_cast<int32>(skeleton->m_InvBind.Num());
    assert(skeleton->m_Parent.Num() == boneCount && "Skeleton parent and invBind arrays must match.");
    if (boneCount == 0 || skeleton->m_Parent.Num() != boneCount)
        return false;

    outGlobalBind.Resize(boneCount);
    outLocalBind.Resize(boneCount);

    for (int32 i = 0; i < boneCount; ++i)
        outGlobalBind[i] = FMath::inverse(skeleton->m_InvBind[i]);

    for (int32 i = 0; i < boneCount; ++i)
    {
        const int32 parent = skeleton->m_Parent[i];
        if (parent >= 0 && parent < boneCount)
            outLocalBind[i] = FMath::inverse(outGlobalBind[parent]) * outGlobalBind[i];
        else
            outLocalBind[i] = outGlobalBind[i];
    }

    return true;
}

inline void BuildGlobalPose(const SkeletonAsset* skeleton, const TArray<Mat4>& localPose, TArray<Mat4>& outGlobalPose)
{
    if (!skeleton)
        return;

    const int32 boneCount = static_cast<int32>(localPose.Num());
    assert(skeleton->m_Parent.Num() == boneCount && "Local pose count must match skeleton parent count.");
    if (boneCount <= 0 || skeleton->m_Parent.Num() != boneCount)
        return;

    outGlobalPose.Resize(boneCount);

    std::vector<uint8> state(static_cast<size_t>(boneCount), 0);

    std::function<void(int32)> resolveBone = [&](int32 boneIndex)
    {
        if (state[boneIndex] == 2)
            return;

        if (state[boneIndex] == 1)
        {
            outGlobalPose[boneIndex] = localPose[boneIndex];
            state[boneIndex] = 2;
            return;
        }

        state[boneIndex] = 1;

        const int32 parent = skeleton->m_Parent[boneIndex];
        if (parent >= 0 && parent < boneCount)
        {
            resolveBone(parent);
            outGlobalPose[boneIndex] = outGlobalPose[parent] * localPose[boneIndex];
        }
        else
        {
            outGlobalPose[boneIndex] = localPose[boneIndex];
        }

        state[boneIndex] = 2;
    };

    for (int32 i = 0; i < boneCount; ++i)
        resolveBone(i);
}

inline Mat4 SampleRootDriverDelta(const AnimationRootDriverData& rootDriver, float sampleTime)
{
    if (!rootDriver.bEnabled)
        return Mat4(1.0f);

    Vector3 translationCurrent(0.0f);
    Vector3 translationBase(0.0f);
    if (rootDriver.bAffectsTranslation)
    {
        translationCurrent = SampleVectorKeys(rootDriver.PositionKeys, sampleTime, Vector3(0.0f));
        if (!rootDriver.PositionKeys.IsEmpty())
            translationBase = rootDriver.PositionKeys[0].Value;
    }

    Quaternion rotationCurrent(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion rotationBase(1.0f, 0.0f, 0.0f, 0.0f);
    if (rootDriver.bAffectsRotation)
    {
        rotationCurrent = SampleRotationKeys(
            rootDriver.RotationKeys,
            sampleTime,
            Quaternion(1.0f, 0.0f, 0.0f, 0.0f));
        if (!rootDriver.RotationKeys.IsEmpty())
            rotationBase = FMath::normalize(rootDriver.RotationKeys[0].Value);
    }

    Vector3 scaleCurrent(1.0f);
    Vector3 scaleBase(1.0f);
    if (rootDriver.bAffectsScale)
    {
        scaleCurrent = SampleVectorKeys(rootDriver.ScaleKeys, sampleTime, Vector3(1.0f));
        if (!rootDriver.ScaleKeys.IsEmpty())
            scaleBase = rootDriver.ScaleKeys[0].Value;
    }

    const Mat4 current = ComposeTRS(translationCurrent, rotationCurrent, scaleCurrent);
    const Mat4 base = ComposeTRS(translationBase, rotationBase, scaleBase);
    return current * FMath::inverse(base);
}

inline bool EvaluateLocalPose(
    const SkeletonAsset* skeleton,
    const AnimationAsset* animation,
    float sampleTime,
    const TArray<Mat4>& localBind,
    TArray<Mat4>& outLocalPose)
{
    if (!skeleton)
        return false;

    const int32 boneCount = static_cast<int32>(localBind.Num());
    assert(skeleton->m_Parent.Num() == boneCount && "Bind pose count must match skeleton parent count.");
    if (boneCount <= 0 || skeleton->m_Parent.Num() != boneCount)
        return false;

    outLocalPose = localBind;

    if (!animation)
        return true;

    for (const AnimationTrack& track : animation->m_Tracks)
    {
        const int32 boneIndex = track.BoneIndex;
        if (boneIndex < 0 || boneIndex >= boneCount)
            continue;

        Vector3 bindTranslation(0.0f);
        Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
        Vector3 bindScale(1.0f);
        DecomposeTRS(localBind[boneIndex], bindTranslation, bindRotation, bindScale);

        const Vector3 translation = SampleVectorKeys(track.PositionKeys, sampleTime, bindTranslation);
        const Quaternion rotation = SampleRotationKeys(track.RotationKeys, sampleTime, bindRotation);
        const Vector3 scale = SampleVectorKeys(track.ScaleKeys, sampleTime, bindScale);

        outLocalPose[boneIndex] = ComposeTRS(translation, rotation, scale);
    }

    if (animation->m_RootDriver.bEnabled)
    {
        const Mat4 rootDelta = SampleRootDriverDelta(animation->m_RootDriver, sampleTime);
        for (int32 i = 0; i < boneCount; ++i)
        {
            const int32 parent = skeleton->m_Parent[i];
            if (parent < 0 || parent >= boneCount)
                outLocalPose[i] = rootDelta * outLocalPose[i];
        }
    }

    return true;
}

inline bool BuildSkinPalette(const SkeletonAsset* skeleton, const TArray<Mat4>& globalPose, TArray<Mat4>& outPalette)
{
    if (!skeleton)
        return false;

    const int32 boneCount = static_cast<int32>(skeleton->m_InvBind.Num());
    assert(skeleton->m_Parent.Num() == boneCount && "Skeleton parent and invBind arrays must match.");
    if (boneCount <= 0 || skeleton->m_Parent.Num() != boneCount || globalPose.Num() != boneCount)
        return false;

    outPalette.Resize(boneCount);
    for (int32 i = 0; i < boneCount; ++i)
        outPalette[i] = globalPose[i] * skeleton->m_InvBind[i];

    return true;
}

inline float MaxAbsDiffFromIdentity(const Mat4& m)
{
    const Mat4 identity(1.0f);
    float maxErr = 0.0f;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
            maxErr = FMath::max(maxErr, std::abs(m[c][r] - identity[c][r]));
    }
    return maxErr;
}

inline float ValidateBindPoseIdentity(
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& globalBindPose,
    int32* outWorstBoneIndex = nullptr)
{
    if (outWorstBoneIndex)
        *outWorstBoneIndex = -1;

    if (!skeleton || skeleton->m_InvBind.Num() != globalBindPose.Num())
        return FLT_MAX;

    float worstError = 0.0f;
    int32 worstBone = -1;
    const int32 boneCount = static_cast<int32>(globalBindPose.Num());
    for (int32 i = 0; i < boneCount; ++i)
    {
        const Mat4 shouldBeIdentity = globalBindPose[i] * skeleton->m_InvBind[i];
        const float error = MaxAbsDiffFromIdentity(shouldBeIdentity);
        if (error > worstError)
        {
            worstError = error;
            worstBone = i;
        }
    }

    if (outWorstBoneIndex)
        *outWorstBoneIndex = worstBone;

    return worstError;
}
}

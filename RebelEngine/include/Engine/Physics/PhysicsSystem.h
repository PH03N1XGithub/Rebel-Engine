#pragma once
#include "Core/CoreTypes.h"
#include "Engine/Physics/Trace.h"

#include <vector>

class Scene;
struct PrimitiveComponent;

class PhysicsSystem
{
public:
    void Init();
    void Shutdown();
    void EditorDebugDraw(Scene& scene);

    void Step(Scene& scene, Float dt);
    void DestroyBodyForComponent(PrimitiveComponent& primitive);

    bool LineTraceSingle(const Vector3& start, const Vector3& end, TraceHit& outHit, const TraceQueryParams& params) const;
    bool LineTraceMulti(const Vector3& start, const Vector3& end, std::vector<TraceHit>& outHits, const TraceQueryParams& params) const;
    bool SphereTraceSingle(const Vector3& start, const Vector3& end, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
    bool CapsuleTraceSingle(const Vector3& start, const Vector3& end, float halfHeight, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
    bool BoxTraceSingle(const Vector3& start, const Vector3& end, const Vector3& halfExtents, const Quaternion& rotation, TraceHit& outHit, const TraceQueryParams& params) const;

    void SetFixedDeltaTime(Float fixedDeltaTime);
    Float GetFixedDeltaTime() const { return m_FixedDT; }

    void SetMaxSubsteps(uint32 maxSubsteps);
    uint32 GetMaxSubsteps() const { return m_MaxSubsteps; }

    uint32 GetLastSubstepCount() const { return m_LastSubstepCount; }

private:
    Float m_Accumulator = 0.0f;
    Float m_FixedDT = 1.0f / 60.0f;
    uint32 m_MaxSubsteps = 8;
    uint32 m_LastSubstepCount = 0;

    struct Impl;
    Impl* m_Impl = nullptr;
};


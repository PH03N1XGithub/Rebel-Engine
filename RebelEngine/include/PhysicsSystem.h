#pragma once
#include "Core/CoreTypes.h"
#include "Trace.h"

#include <vector>

class Scene;

class PhysicsSystem
{
public:
	void Init();
	void Shutdown();
	void EditorDebugDraw(Scene& scene);

	void Step(Scene& scene, Float dt);

	bool LineTraceSingle(const Vector3& start, const Vector3& end, TraceHit& outHit, const TraceQueryParams& params) const;
	bool LineTraceMulti(const Vector3& start, const Vector3& end, std::vector<TraceHit>& outHits, const TraceQueryParams& params) const;
	bool SphereTraceSingle(const Vector3& start, const Vector3& end, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
	bool CapsuleTraceSingle(const Vector3& start, const Vector3& end, float halfHeight, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
	bool BoxTraceSingle(const Vector3& start, const Vector3& end, const Vector3& halfExtents, const Quaternion& rotation, TraceHit& outHit, const TraceQueryParams& params) const;

private:
	Float m_Accumulator = 0.0f;
	Float m_FixedDT = 1.0f / 60.0f;

	struct Impl;
	Impl* m = nullptr;
};

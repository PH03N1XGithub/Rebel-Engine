#include "EnginePch.h"
#include "Trace.h"

#include "BaseEngine.h"
#include "PhysicsDebugDraw.h"
#include "PhysicsModule.h"
#include "PhysicsSystem.h"

#include <glm/gtc/constants.hpp>

namespace
{
	const Vector3 kTraceColor = {0.1f, 0.9f, 0.1f};
	const Vector3 kHitColor = {0.9f, 0.1f, 0.1f};
	const Vector3 kNormalColor = {1.0f, 1.0f, 0.0f};

	PhysicsSystem* GetActivePhysicsSystem()
	{
		if (!GEngine)
			return nullptr;

		PhysicsModule* physicsModule = GEngine->GetModuleManager().GetModule<PhysicsModule>();
		if (!physicsModule)
			return nullptr;

		return physicsModule->GetPhysicsSystem();
	}

	TraceQueryParams BuildParams(CollisionChannel channel)
	{
		TraceQueryParams params;
		params.Channel = channel;
		return params;
	}

	bool ShouldDrawDebug(EDrawDebugTrace::Type drawDebugType)
	{
		return drawDebugType != EDrawDebugTrace::None;
	}

	void DrawWireSphere(const Vector3& center, float radius, const Vector3& color)
	{
		constexpr int segments = 16;
		for (int i = 0; i < segments; ++i)
		{
			const float a0 = (i / (float)segments) * glm::two_pi<float>();
			const float a1 = ((i + 1) / (float)segments) * glm::two_pi<float>();

			PhysicsDebug::DrawLine(
				center + Vector3(cosf(a0) * radius, sinf(a0) * radius, 0.0f),
				center + Vector3(cosf(a1) * radius, sinf(a1) * radius, 0.0f),
				color);
			PhysicsDebug::DrawLine(
				center + Vector3(cosf(a0) * radius, 0.0f, sinf(a0) * radius),
				center + Vector3(cosf(a1) * radius, 0.0f, sinf(a1) * radius),
				color);
			PhysicsDebug::DrawLine(
				center + Vector3(0.0f, cosf(a0) * radius, sinf(a0) * radius),
				center + Vector3(0.0f, cosf(a1) * radius, sinf(a1) * radius),
				color);
		}
	}

	void DrawWireCapsule(const Vector3& center, float halfHeight, float radius, const Vector3& color)
	{
		const Vector3 top = center + Vector3(0.0f, 0.0f, halfHeight);
		const Vector3 bottom = center - Vector3(0.0f, 0.0f, halfHeight);

		PhysicsDebug::DrawLine(top + Vector3(radius, 0.0f, 0.0f), bottom + Vector3(radius, 0.0f, 0.0f), color);
		PhysicsDebug::DrawLine(top - Vector3(radius, 0.0f, 0.0f), bottom - Vector3(radius, 0.0f, 0.0f), color);
		PhysicsDebug::DrawLine(top + Vector3(0.0f, radius, 0.0f), bottom + Vector3(0.0f, radius, 0.0f), color);
		PhysicsDebug::DrawLine(top - Vector3(0.0f, radius, 0.0f), bottom - Vector3(0.0f, radius, 0.0f), color);

		DrawWireSphere(top, radius, color);
		DrawWireSphere(bottom, radius, color);
	}
}

bool LineTraceSingle(
	const Vector3& start,
	const Vector3& end,
	TraceHit& outHit,
	CollisionChannel channel,
	EDrawDebugTrace::Type drawDebugType)
{
	return LineTraceSingle(start, end, outHit, BuildParams(channel), drawDebugType);
}

bool LineTraceSingle(
	const Vector3& start,
	const Vector3& end,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType)
{
	PhysicsSystem* physicsSystem = GetActivePhysicsSystem();
	if (!physicsSystem)
	{
		outHit = {};
		if (ShouldDrawDebug(drawDebugType))
			DrawDebugLineTrace(start, end, nullptr);
		return false;
	}

	const bool bHit = physicsSystem->LineTraceSingle(start, end, outHit, params);
	if (ShouldDrawDebug(drawDebugType))
		DrawDebugLineTrace(start, end, bHit ? &outHit : nullptr);
	return bHit;
}

bool LineTraceMulti(
	const Vector3& start,
	const Vector3& end,
	std::vector<TraceHit>& outHits,
	CollisionChannel channel,
	EDrawDebugTrace::Type drawDebugType)
{
	return LineTraceMulti(start, end, outHits, BuildParams(channel), drawDebugType);
}

bool LineTraceMulti(
	const Vector3& start,
	const Vector3& end,
	std::vector<TraceHit>& outHits,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType)
{
	PhysicsSystem* physicsSystem = GetActivePhysicsSystem();
	if (!physicsSystem)
	{
		outHits.clear();
		if (ShouldDrawDebug(drawDebugType))
			DrawDebugLineTrace(start, end, nullptr);
		return false;
	}

	const bool bHit = physicsSystem->LineTraceMulti(start, end, outHits, params);
	if (ShouldDrawDebug(drawDebugType))
	{
		DrawDebugLineTrace(start, end, bHit && !outHits.empty() ? &outHits[0] : nullptr);
		for (size_t i = 1; i < outHits.size(); ++i)
			DrawDebugHitNormal(outHits[i], 0.2f);
	}
	return bHit;
}

bool SphereTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float radius,
	TraceHit& outHit,
	CollisionChannel channel,
	EDrawDebugTrace::Type drawDebugType)
{
	return SphereTraceSingle(start, end, radius, outHit, BuildParams(channel), drawDebugType);
}

bool SphereTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float radius,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType)
{
	PhysicsSystem* physicsSystem = GetActivePhysicsSystem();
	if (!physicsSystem)
	{
		outHit = {};
		if (ShouldDrawDebug(drawDebugType))
			DrawDebugSphereTrace(start, end, radius, nullptr);
		return false;
	}

	const bool bHit = physicsSystem->SphereTraceSingle(start, end, radius, outHit, params);
	if (ShouldDrawDebug(drawDebugType))
		DrawDebugSphereTrace(start, end, radius, bHit ? &outHit : nullptr);
	return bHit;
}

bool CapsuleTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float halfHeight,
	float radius,
	TraceHit& outHit,
	CollisionChannel channel,
	EDrawDebugTrace::Type drawDebugType)
{
	return CapsuleTraceSingle(start, end, halfHeight, radius, outHit, BuildParams(channel), drawDebugType);
}

bool CapsuleTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float halfHeight,
	float radius,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType)
{
	PhysicsSystem* physicsSystem = GetActivePhysicsSystem();
	if (!physicsSystem)
	{
		outHit = {};
		if (ShouldDrawDebug(drawDebugType))
			DrawDebugCapsuleTrace(start, end, halfHeight, radius, nullptr);
		return false;
	}

	const bool bHit = physicsSystem->CapsuleTraceSingle(start, end, halfHeight, radius, outHit, params);
	if (ShouldDrawDebug(drawDebugType))
		DrawDebugCapsuleTrace(start, end, halfHeight, radius, bHit ? &outHit : nullptr);
	return bHit;
}

bool BoxTraceSingle(
	const Vector3& start,
	const Vector3& end,
	const Vector3& halfExtents,
	const Quaternion& rotation,
	TraceHit& outHit,
	CollisionChannel channel,
	EDrawDebugTrace::Type drawDebugType)
{
	return BoxTraceSingle(start, end, halfExtents, rotation, outHit, BuildParams(channel), drawDebugType);
}

bool BoxTraceSingle(
	const Vector3& start,
	const Vector3& end,
	const Vector3& halfExtents,
	const Quaternion& rotation,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType)
{
	PhysicsSystem* physicsSystem = GetActivePhysicsSystem();
	if (!physicsSystem)
	{
		outHit = {};
		if (ShouldDrawDebug(drawDebugType))
			DrawDebugLineTrace(start, end, nullptr);
		return false;
	}

	const bool bHit = physicsSystem->BoxTraceSingle(start, end, halfExtents, rotation, outHit, params);
	if (ShouldDrawDebug(drawDebugType))
		DrawDebugLineTrace(start, end, bHit ? &outHit : nullptr);
	return bHit;
}

void DrawDebugLineTrace(const Vector3& start, const Vector3& end, const TraceHit* hit)
{
	if (hit != nullptr && (hit->HitEntity != entt::null || hit->bBlockingHit))
	{
		PhysicsDebug::DrawLine(start, hit->Position, kHitColor);
		PhysicsDebug::DrawLine(hit->Position, end, kTraceColor);
		DrawDebugHitNormal(*hit, 0.2f);
	}
	else
	{
		PhysicsDebug::DrawLine(start, end, kTraceColor);
	}
}

void DrawDebugSphereTrace(const Vector3& start, const Vector3& end, float radius, const TraceHit* hit)
{
	DrawWireSphere(start, radius, kTraceColor);
	DrawWireSphere(end, radius, kTraceColor);
	PhysicsDebug::DrawLine(start, end, kTraceColor);

	if (hit != nullptr && (hit->HitEntity != entt::null || hit->bBlockingHit))
	{
		DrawWireSphere(hit->Position, radius * 0.25f, kHitColor);
		DrawDebugHitNormal(*hit, 0.2f);
	}
}

void DrawDebugCapsuleTrace(const Vector3& start, const Vector3& end, float halfHeight, float radius, const TraceHit* hit)
{
	DrawWireCapsule(start, halfHeight, radius, kTraceColor);
	DrawWireCapsule(end, halfHeight, radius, kTraceColor);
	PhysicsDebug::DrawLine(start, end, kTraceColor);

	if (hit != nullptr && (hit->HitEntity != entt::null || hit->bBlockingHit))
	{
		DrawWireSphere(hit->Position, radius * 0.25f, kHitColor);
		DrawDebugHitNormal(*hit, 0.2f);
	}
}

void DrawDebugHitNormal(const TraceHit& hit, float length)
{
	PhysicsDebug::DrawLine(hit.Position, hit.Position + hit.Normal * length, kNormalColor);
}

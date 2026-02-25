#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/CoreMath.h"

#include <ThirdParty/entt.h>

#include <cstddef>
#include <vector>

using EntityID = entt::entity;

enum class CollisionChannel : uint8
{
	WorldStatic = 0,
	WorldDynamic,
	Pawn,
	Visibility,
	Camera,
	Trigger,
	Any
};

namespace EDrawDebugTrace
{
	enum Type : uint8
	{
		None = 0,
		ForOneFrame,
		ForDuration,
		Persistent
	};
}

struct TraceHit
{
	bool bBlockingHit = false;
	Vector3 Position{0.0f};
	Vector3 Normal{0.0f, 0.0f, 1.0f};
	float Distance = 0.0f;
	EntityID HitEntity = entt::null;
};

struct TraceQueryParams
{
	CollisionChannel Channel = CollisionChannel::Any;
	const EntityID* IgnoreEntities = nullptr;
	size_t IgnoreEntityCount = 0;
	bool bTraceTriggers = false;
};

bool LineTraceSingle(
	const Vector3& start,
	const Vector3& end,
	TraceHit& outHit,
	CollisionChannel channel = CollisionChannel::WorldStatic,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool LineTraceSingle(
	const Vector3& start,
	const Vector3& end,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool LineTraceMulti(
	const Vector3& start,
	const Vector3& end,
	std::vector<TraceHit>& outHits,
	CollisionChannel channel = CollisionChannel::Any,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool LineTraceMulti(
	const Vector3& start,
	const Vector3& end,
	std::vector<TraceHit>& outHits,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool SphereTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float radius,
	TraceHit& outHit,
	CollisionChannel channel = CollisionChannel::WorldStatic,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool SphereTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float radius,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool CapsuleTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float halfHeight,
	float radius,
	TraceHit& outHit,
	CollisionChannel channel = CollisionChannel::WorldStatic,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool CapsuleTraceSingle(
	const Vector3& start,
	const Vector3& end,
	float halfHeight,
	float radius,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool BoxTraceSingle(
	const Vector3& start,
	const Vector3& end,
	const Vector3& halfExtents,
	const Quaternion& rotation,
	TraceHit& outHit,
	CollisionChannel channel = CollisionChannel::WorldStatic,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

bool BoxTraceSingle(
	const Vector3& start,
	const Vector3& end,
	const Vector3& halfExtents,
	const Quaternion& rotation,
	TraceHit& outHit,
	const TraceQueryParams& params,
	EDrawDebugTrace::Type drawDebugType = EDrawDebugTrace::None
);

void DrawDebugLineTrace(
	const Vector3& start,
	const Vector3& end,
	const TraceHit* hit = nullptr
);

void DrawDebugSphereTrace(
	const Vector3& start,
	const Vector3& end,
	float radius,
	const TraceHit* hit = nullptr
);

void DrawDebugCapsuleTrace(
	const Vector3& start,
	const Vector3& end,
	float halfHeight,
	float radius,
	const TraceHit* hit = nullptr
);

void DrawDebugHitNormal(
	const TraceHit& hit,
	float length = 0.2f
);

// Example usage:
// TraceHit hit;
// LineTraceSingle(feetPos, feetPos - up * 0.2f, hit);
// SphereTraceSingle(pos, pos + forward * 0.5f, 0.3f, hit);
// std::vector<TraceHit> hits;
// LineTraceMulti(muzzle, muzzle + forward * 100.0f, hits);

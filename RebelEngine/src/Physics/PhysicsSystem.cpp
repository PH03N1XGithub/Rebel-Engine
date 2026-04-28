#include "Engine/Framework/EnginePch.h"
#include "Engine/Physics/PhysicsSystem.h"

#include "Engine/Components/Components.h"
#include "Engine/Physics/PhysicsDebugDraw.h"
#include "Engine/Scene/Scene.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <thread>
#include <type_traits>

namespace
{
	constexpr float kSmallTraceDistance = 1.0e-5f;

	constexpr JPH::ObjectLayer kLayerWorldStatic = 0;
	constexpr JPH::ObjectLayer kLayerWorldDynamic = 1;
	constexpr JPH::ObjectLayer kLayerPawn = 2;
	constexpr JPH::ObjectLayer kLayerVisibility = 3;
	constexpr JPH::ObjectLayer kLayerCamera = 4;
	constexpr JPH::ObjectLayer kLayerTrigger = 5;

	class SimpleBroadPhaseLayerInterface : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return 1;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override
		{
			return JPH::BroadPhaseLayer(0);
		}

		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override
		{
			return "DEFAULT";
		}
	};

	class SimpleObjectLayerPairFilter : public JPH::ObjectLayerPairFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override
		{
			return true;
		}
	};

	class SimpleObjectVsBroadPhaseLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override
		{
			return true;
		}
	};

	inline JPH::Vec3 ToJolt(const Vector3& v)
	{
		return {v.x, v.y, v.z};
	}

	inline JPH::RVec3 ToJoltR(const Vector3& v)
	{
		return JPH::RVec3(v.x, v.y, v.z);
	}

	inline JPH::Quat ToJoltQ(const Quaternion& v)
	{
		return JPH::Quat(v.x, v.y, v.z, v.w);
	}

	inline Vector3 ToRebel(const JPH::Vec3& v)
	{
		return {v.GetX(), v.GetY(), v.GetZ()};
	}

	inline Vector3 ToRebel(const JPH::RVec3& v)
	{
		return Vector3((float)v.GetX(), (float)v.GetY(), (float)v.GetZ());
	}

	inline Quaternion ToRebel(const JPH::Quat& v)
	{
		return Quaternion((float)v.GetW(), (float)v.GetX(), (float)v.GetY(), (float)v.GetZ());
	}

	inline JPH::ObjectLayer ToObjectLayer(CollisionChannel channel)
	{
		switch (channel)
		{
		case CollisionChannel::WorldStatic: return kLayerWorldStatic;
		case CollisionChannel::WorldDynamic: return kLayerWorldDynamic;
		case CollisionChannel::Pawn: return kLayerPawn;
		case CollisionChannel::Visibility: return kLayerVisibility;
		case CollisionChannel::Camera: return kLayerCamera;
		case CollisionChannel::Trigger: return kLayerTrigger;
		case CollisionChannel::Any: break;
		}

		return kLayerWorldDynamic;
	}

	inline bool ChannelMatchesLayer(CollisionChannel channel, JPH::ObjectLayer layer)
	{
		if (channel == CollisionChannel::Any)
			return true;

		return layer == ToObjectLayer(channel);
	}

	inline CollisionChannel ResolveBodyCollisionChannel(const PrimitiveComponent& primitive)
	{
		if (primitive.ObjectChannel != CollisionChannel::Any)
			return primitive.ObjectChannel;

		return primitive.BodyType == ERBBodyType::Static
			? CollisionChannel::WorldStatic
			: CollisionChannel::WorldDynamic;
	}

	inline uint64 EncodeEntityID(EntityID entity)
	{
		return static_cast<uint64>(entt::to_integral(entity));
	}

	inline EntityID DecodeEntityID(uint64 value)
	{
		using EntityUnderlying = std::underlying_type_t<EntityID>;
		return static_cast<EntityID>(static_cast<EntityUnderlying>(value));
	}

	class TraceObjectLayerFilter final : public JPH::ObjectLayerFilter
	{
	public:
		explicit TraceObjectLayerFilter(CollisionChannel channel)
			: m_Channel(channel)
		{
		}

		bool ShouldCollide(JPH::ObjectLayer layer) const override
		{
			return ChannelMatchesLayer(m_Channel, layer);
		}

	private:
		CollisionChannel m_Channel;
	};

	class TraceBodyFilter final : public JPH::BodyFilter
	{
	public:
		explicit TraceBodyFilter(const TraceQueryParams& params)
			: m_Params(params)
		{
		}

		bool ShouldCollideLocked(const JPH::Body& body) const override
		{
			if (!m_Params.bTraceTriggers && body.IsSensor())
				return false;

			if (m_Params.IgnoreEntities == nullptr || m_Params.IgnoreEntityCount == 0)
				return true;

			const EntityID entity = DecodeEntityID(body.GetUserData());
			for (size_t i = 0; i < m_Params.IgnoreEntityCount; ++i)
			{
				if (m_Params.IgnoreEntities[i] == entity)
					return false;
			}

			return true;
		}

	private:
		const TraceQueryParams& m_Params;
	};

	bool FillTraceHitFromRay(
		const JPH::PhysicsSystem& physicsSystem,
		const JPH::RRayCast& ray,
		float totalDistance,
		const JPH::RayCastResult& rayHit,
		TraceHit& outHit)
	{
		const JPH::BodyLockInterface& lockInterface = physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(lockInterface, rayHit.mBodyID);
		if (!lock.Succeeded())
			return false;

		const JPH::Body& body = lock.GetBody();
		const JPH::RVec3 hitPosition = ray.GetPointOnRay(rayHit.mFraction);

		outHit.bBlockingHit = !body.IsSensor();
		outHit.Position = ToRebel(hitPosition);
		outHit.Normal = ToRebel(body.GetWorldSpaceSurfaceNormal(rayHit.mSubShapeID2, hitPosition));
		outHit.Distance = rayHit.mFraction * totalDistance;
		outHit.HitEntity = DecodeEntityID(body.GetUserData());
		return true;
	}

	bool FillTraceHitFromShapeCast(
		const JPH::PhysicsSystem& physicsSystem,
		float totalDistance,
		const JPH::ShapeCastResult& shapeHit,
		TraceHit& outHit)
	{
		const JPH::BodyLockInterface& lockInterface = physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(lockInterface, shapeHit.mBodyID2);
		if (!lock.Succeeded())
			return false;

		const JPH::Body& body = lock.GetBody();
		const JPH::RVec3 hitPosition(shapeHit.mContactPointOn2);

		Vector3 normal = ToRebel(body.GetWorldSpaceSurfaceNormal(shapeHit.mSubShapeID2, hitPosition));
		const Vector3 penetrationAxis = ToRebel(shapeHit.mPenetrationAxis);
		if (glm::dot(normal, normal) <= 0.0f && glm::dot(penetrationAxis, penetrationAxis) > 0.0f)
			normal = -glm::normalize(penetrationAxis);

		outHit.bBlockingHit = !body.IsSensor();
		outHit.Position = ToRebel(hitPosition);
		outHit.Normal = normal;
		outHit.Distance = shapeHit.mFraction * totalDistance;
		outHit.HitEntity = DecodeEntityID(body.GetUserData());
		return true;
	}

	bool CastShapeSingleInternal(
		const JPH::PhysicsSystem& physicsSystem,
		const JPH::Shape& shape,
		const Vector3& start,
		const Quaternion& startRotation,
		const Vector3& displacement,
		const TraceQueryParams& params,
		TraceHit& outHit)
	{
		outHit = {};

		const float distance = glm::length(displacement);
		if (distance <= kSmallTraceDistance)
			return false;

		TraceObjectLayerFilter objectFilter(params.Channel);
		TraceBodyFilter bodyFilter(params);

		const JPH::RMat44 castStart = JPH::RMat44::sRotationTranslation(ToJoltQ(startRotation), ToJoltR(start));
		const JPH::RShapeCast shapeCast(&shape, JPH::Vec3::sReplicate(1.0f), castStart, ToJolt(displacement));

		JPH::ShapeCastSettings settings;
		JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;

		physicsSystem.GetNarrowPhaseQuery().CastShape(
			shapeCast,
			settings,
			JPH::RVec3::sZero(),
			collector,
			{},
			objectFilter,
			bodyFilter);

		if (!collector.HadHit())
			return false;

		return FillTraceHitFromShapeCast(physicsSystem, distance, collector.mHit, outHit);
	}

	static JPH::Ref<JPH::Shape> BuildJoltShape(
		const PhysicsShape& shape,
		const PrimitiveComponent* pc)
	{
		const Vector3 worldScale = pc ? pc->GetWorldScale() : Vector3(1.0f, 1.0f, 1.0f);

		switch (shape.Type)
		{
		case EPhysicsShapeType::Box:
		{
			const Vector3 halfExtents = shape.HalfExtent * worldScale;
			JPH::BoxShapeSettings settings(ToJolt(halfExtents));
			auto result = settings.Create();
			if (result.HasError())
				return nullptr;

			return result.Get();
		}
		case EPhysicsShapeType::Sphere:
		{
			const float radiusScale = std::max(worldScale.x, worldScale.y);
			const float radius = shape.Radius * radiusScale;
			JPH::SphereShapeSettings settings(radius);
			auto result = settings.Create();
			if (result.HasError())
				return nullptr;

			return result.Get();
		}
		case EPhysicsShapeType::Capsule:
		{
			const float radiusScale = std::max(worldScale.x, worldScale.y);
			const float radius = shape.Radius * radiusScale;
			float engineHalfHeight = shape.HalfHeight * worldScale.z;
			float capsuleHalfHeight = std::max(engineHalfHeight - radius, 0.0f);

			JPH::CapsuleShapeSettings capsuleSettings(capsuleHalfHeight, radius);
			auto capsuleResult = capsuleSettings.Create();
			if (capsuleResult.HasError())
				return nullptr;

			const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::DegreesToRadians(90.0f));
			JPH::RotatedTranslatedShapeSettings transformSettings(JPH::Vec3::sZero(), rotation, capsuleResult.Get());
			auto transformedResult = transformSettings.Create();
			if (transformedResult.HasError())
				return nullptr;

			return transformedResult.Get();
		}
		}

		return nullptr;
	}
}

struct PhysicsSystem::Impl
{
	SimpleBroadPhaseLayerInterface BroadPhase;
	SimpleObjectLayerPairFilter LayerPairFilter;
	SimpleObjectVsBroadPhaseLayerFilter ObjectVsBPFilter;

	JPH::PhysicsSystem System;
	JPH::TempAllocatorImpl* TempAllocator = nullptr;
	JPH::JobSystemThreadPool* JobSystem = nullptr;
};

DEFINE_LOG_CATEGORY(PhysicsSystemLog)

void PhysicsSystem::Init()
{
	JPH::RegisterDefaultAllocator();
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_Impl = new Impl();
	m_Impl->TempAllocator = new JPH::TempAllocatorImpl(5 * 1024 * 1024);
	m_Impl->JobSystem = new JPH::JobSystemThreadPool(
		JPH::cMaxPhysicsJobs,
		JPH::cMaxPhysicsBarriers,
		std::thread::hardware_concurrency());

	m_Impl->System.Init(
		1024,
		0,
		1024,
		1024,
		m_Impl->BroadPhase,
		m_Impl->ObjectVsBPFilter,
		m_Impl->LayerPairFilter);
	m_Impl->System.SetGravity({0.0f, 0.0f, -9.81f});
}

void PhysicsSystem::Shutdown()
{
	if (m_Impl != nullptr)
	{
		delete m_Impl->JobSystem;
		delete m_Impl->TempAllocator;
		delete m_Impl;
		m_Impl = nullptr;
	}

	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::SetFixedDeltaTime(Float fixedDeltaTime)
{
	if (fixedDeltaTime <= 0.0f)
		return;

	m_FixedDT = fixedDeltaTime;
	const Float maxAccumulatedTime = m_FixedDT * static_cast<Float>(m_MaxSubsteps);
	if (m_Accumulator > maxAccumulatedTime)
		m_Accumulator = maxAccumulatedTime;
}

void PhysicsSystem::SetMaxSubsteps(uint32 maxSubsteps)
{
	m_MaxSubsteps = std::max<uint32>(1u, maxSubsteps);
	const Float maxAccumulatedTime = m_FixedDT * static_cast<Float>(m_MaxSubsteps);
	if (m_Accumulator > maxAccumulatedTime)
		m_Accumulator = maxAccumulatedTime;
}
void PhysicsSystem::DestroyBodyForComponent(PrimitiveComponent& primitive)
{
	if (!primitive.IsBodyCreated())
	{
		primitive.ClearBodyHandle();
		return;
	}

	if (primitive.GetBodyHandle() == 0 || m_Impl == nullptr)
	{
		primitive.ClearBodyHandle();
		return;
	}

	auto& bodies = m_Impl->System.GetBodyInterface();
	const JPH::BodyID bodyID(primitive.GetBodyHandle());
	if (bodyID.IsInvalid())
	{
		primitive.ClearBodyHandle();
		return;
	}

	{
		const JPH::BodyLockInterface& lockInterface = m_Impl->System.GetBodyLockInterface();
		JPH::BodyLockRead lock(lockInterface, bodyID);
		if (!lock.Succeeded())
		{
			primitive.ClearBodyHandle();
			return;
		}
	}

	if (bodies.IsAdded(bodyID))
		bodies.RemoveBody(bodyID);

	bodies.DestroyBody(bodyID);
	primitive.ClearBodyHandle();
}

static void DebugDrawHalfSphereAt(
	const Vector3& center,
	float radius,
	const Vector3& up,
	const Vector3& right,
	const Vector3& forward,
	const Vector3& color)
{
	constexpr int segments = 16;     // smoothness of arcs
	constexpr int arcCount = 4;      // number of vertical arcs (like Unreal)

	// --- Vertical arcs ---
	for (int arc = 0; arc < arcCount; ++arc)
	{
		float phi = (arc / (float)arcCount) * glm::two_pi<float>();

		Vector3 arcDir =
			cosf(phi) * right +
			sinf(phi) * forward;

		for (int i = 0; i < segments; ++i)
		{
			float t0 = (i / (float)segments) * glm::half_pi<float>();        // 0 â†’ 90Â°
			float t1 = ((i + 1) / (float)segments) * glm::half_pi<float>();

			Vector3 p0 =
				center +
				arcDir * sinf(t0) * radius +
				up     * cosf(t0) * radius;

			Vector3 p1 =
				center +
				arcDir * sinf(t1) * radius +
				up     * cosf(t1) * radius;

			PhysicsDebug::DrawLine(p0, p1, color);
		}
	}

	// --- Base circle (equator) ---
	for (int i = 0; i < segments; ++i)
	{
		float a0 = (i / (float)segments) * glm::two_pi<float>();
		float a1 = ((i + 1) / (float)segments) * glm::two_pi<float>();

		Vector3 p0 =
			center +
			(cosf(a0) * right + sinf(a0) * forward) * radius;

		Vector3 p1 =
			center +
			(cosf(a1) * right + sinf(a1) * forward) * radius;

		PhysicsDebug::DrawLine(p0, p1, color);
	}
}

static void DebugDrawSphereAt(const Vector3& center, float radius, const Vector3& color)
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

static void DebugDrawSphere(const PhysicsShape& shape, const PrimitiveComponent* primitive, const Vector3& color)
{
	const Vector3 worldScale = primitive ? primitive->GetWorldScale() : Vector3(1.0f, 1.0f, 1.0f);
	const float radius = shape.Radius * std::max(worldScale.x, worldScale.y);
	DebugDrawSphereAt(primitive->GetWorldPosition(), radius, color);
}

static void DebugDrawCapsule(const PhysicsShape& shape,
							 const PrimitiveComponent* primitive,
							 const Vector3& color)
{
	const Vector3 worldScale = primitive ? primitive->GetWorldScale() : Vector3(1.0f, 1.0f, 1.0f);

	// Match BuildJoltShape: capsule radius uses XY scale and half-height uses Z scale.
	const float radius = shape.Radius * std::max(worldScale.x, worldScale.y);
	const float halfHeight = shape.HalfHeight * worldScale.z;
	const float cylHalf = std::max(0.0f, halfHeight - radius);

	const glm::quat q = primitive->GetWorldRotationQuat();
	const Vector3 up      = glm::rotate(q, Vector3(0.0f, 0.0f, 1.0f));
	const Vector3 right   = glm::rotate(q, Vector3(1.0f, 0.0f, 0.0f));
	const Vector3 forward = glm::rotate(q, Vector3(0.0f, 1.0f, 0.0f));

	const Vector3 center = primitive->GetWorldPosition();
	const Vector3 top    = center + up * cylHalf;     // sphere center
	const Vector3 bottom = center - up * cylHalf;     // sphere center

	// Draw the "side lines" of the cylinder portion
	PhysicsDebug::DrawLine(top + right   * radius, bottom + right   * radius, color);
	PhysicsDebug::DrawLine(top - right   * radius, bottom - right   * radius, color);
	PhysicsDebug::DrawLine(top + forward * radius, bottom + forward * radius, color);
	PhysicsDebug::DrawLine(top - forward * radius, bottom - forward * radius, color);

	// Draw caps as spheres at the correct centers
	DebugDrawHalfSphereAt(top, radius, up, right, forward, color);
	DebugDrawHalfSphereAt(bottom, radius, -up, right, forward, color);
}

static void DebugDrawBox(const PhysicsShape& shape, const PrimitiveComponent* primitive, const Vector3& color)
{
	const Vector3 worldScale = primitive ? primitive->GetWorldScale() : Vector3(1.0f, 1.0f, 1.0f);
	const Vector3 he = shape.HalfExtent * worldScale;
	const glm::quat q = primitive->GetWorldRotationQuat();
	const Mat4 rotation = glm::toMat4(q);

	auto TransformPoint = [&](const Vector3& p) { return Vector3(rotation * Vector4(p, 1.0f)) + primitive->GetWorldPosition(); };

	Vector3 p[8] = {
		TransformPoint({-he.x, -he.y, -he.z}),
		TransformPoint({ he.x, -he.y, -he.z}),
		TransformPoint({ he.x,  he.y, -he.z}),
		TransformPoint({-he.x,  he.y, -he.z}),
		TransformPoint({-he.x, -he.y,  he.z}),
		TransformPoint({ he.x, -he.y,  he.z}),
		TransformPoint({ he.x,  he.y,  he.z}),
		TransformPoint({-he.x,  he.y,  he.z}),
	};

	PhysicsDebug::DrawLine(p[0], p[1], color);
	PhysicsDebug::DrawLine(p[1], p[2], color);
	PhysicsDebug::DrawLine(p[2], p[3], color);
	PhysicsDebug::DrawLine(p[3], p[0], color);
	PhysicsDebug::DrawLine(p[4], p[5], color);
	PhysicsDebug::DrawLine(p[5], p[6], color);
	PhysicsDebug::DrawLine(p[6], p[7], color);
	PhysicsDebug::DrawLine(p[7], p[4], color);
	PhysicsDebug::DrawLine(p[0], p[4], color);
	PhysicsDebug::DrawLine(p[1], p[5], color);
	PhysicsDebug::DrawLine(p[2], p[6], color);
	PhysicsDebug::DrawLine(p[3], p[7], color);
}

void PhysicsSystem::EditorDebugDraw(Scene& scene)
{
	auto view = scene.GetRegistry().view<PrimitiveComponent*>();
	for (auto entity : view)
	{
		PrimitiveComponent* primitive = view.get<PrimitiveComponent*>(entity);
		if (!primitive)
			continue;

		const PhysicsShape shape = primitive->CreatePhysicsShape();

		Vector3 color;
		switch (primitive->BodyType)
		{
		case ERBBodyType::Static: color = {1.0f, 0.6f, 0.0f}; break;
		case ERBBodyType::Dynamic: color = {0.0f, 1.0f, 0.0f}; break;
		case ERBBodyType::Kinematic: color = {0.0f, 0.0f, 1.0f}; break;
		}

		switch (shape.Type)
		{
		// Use primitive world transform so debug view matches runtime body placement.
		case EPhysicsShapeType::Box: DebugDrawBox(shape, primitive, color); break;
		case EPhysicsShapeType::Sphere: DebugDrawSphere(shape, primitive, color); break;
		case EPhysicsShapeType::Capsule: DebugDrawCapsule(shape, primitive, color); break;
		}
	}
}

void PhysicsSystem::Step(Scene& scene, Float dt)
{
	if (m_Impl == nullptr)
		return;

	auto& registry = scene.GetRegistry();
	auto& bodies = m_Impl->System.GetBodyInterface();

	auto view = registry.view<PrimitiveComponent*>();
	for (auto entity : view)
	{
		PrimitiveComponent* primitive = view.get<PrimitiveComponent*>(entity);
		if (!primitive)
			continue;

		Actor* owner = primitive->GetOwner();
		if (!owner || owner->IsPendingDestroy())
		{
			DestroyBodyForComponent(*primitive);
			continue;
		}

		if (primitive->IsBodyCreated())
			continue;

		const PhysicsShape engineShape = primitive->CreatePhysicsShape();
		JPH::Ref<JPH::Shape> joltShape = BuildJoltShape(engineShape, primitive);
		if (joltShape == nullptr)
			continue;

		JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
		switch (primitive->BodyType)
		{
		case ERBBodyType::Static: motionType = JPH::EMotionType::Static; break;
		case ERBBodyType::Dynamic: motionType = JPH::EMotionType::Dynamic; break;
		case ERBBodyType::Kinematic: motionType = JPH::EMotionType::Kinematic; break;
		}

		if (primitive->BodyType == ERBBodyType::Kinematic)
		{
			RB_LOG(PhysicsSystemLog, info, "test")
		}

		const CollisionChannel channel = ResolveBodyCollisionChannel(*primitive);
		const JPH::ObjectLayer objectLayer = ToObjectLayer(channel);

		// Spawn body from the owning primitive's world transform (not actor root).
		JPH::BodyCreationSettings settings(
			joltShape,
			ToJoltR(primitive->GetWorldPosition()),
			ToJoltQ(primitive->GetWorldRotationQuat()),
			motionType,
			objectLayer);

		settings.mIsSensor = primitive->bIsTrigger || channel == CollisionChannel::Trigger;
		settings.mUserData = EncodeEntityID(owner->GetHandle());

		JPH::Body* body = bodies.CreateBody(settings);
		if (!body)
			continue;

		const JPH::EActivation activation = motionType == JPH::EMotionType::Dynamic
			? JPH::EActivation::Activate
			: JPH::EActivation::DontActivate;
		bodies.AddBody(body->GetID(), activation);

		primitive->SetBodyHandle(body->GetID().GetIndexAndSequenceNumber());
	}

	const Float safeDt = std::max<Float>(0.0f, dt);
	const Float maxAccumulatedTime = m_FixedDT * static_cast<Float>(m_MaxSubsteps);
	m_Accumulator = std::min(m_Accumulator + safeDt, maxAccumulatedTime);
	m_LastSubstepCount = 0;

	while (m_Accumulator >= m_FixedDT && m_LastSubstepCount < m_MaxSubsteps)
	{
		for (auto entity : view)
		{
			PrimitiveComponent* primitive = view.get<PrimitiveComponent*>(entity);
			if (!primitive)
				continue;

			if (primitive->BodyType != ERBBodyType::Kinematic)
				continue;

			if (!primitive->IsBodyCreated() || primitive->GetBodyHandle() == 0)
				continue;

			Actor* owner = primitive->GetOwner();
			if (!owner || owner->IsPendingDestroy())
			{
				DestroyBodyForComponent(*primitive);
				continue;
			}

			const JPH::BodyID bodyID(primitive->GetBodyHandle());
			// Drive kinematic bodies from the owning primitive's world transform.
			bodies.MoveKinematic(bodyID, ToJoltR(primitive->GetWorldPosition()), ToJoltQ(primitive->GetWorldRotationQuat()), m_FixedDT);
		}

		m_Impl->System.Update(m_FixedDT, 8, m_Impl->TempAllocator, m_Impl->JobSystem);
		m_Accumulator -= m_FixedDT;
		++m_LastSubstepCount;
	}

	for (auto entity : view)
	{
		PrimitiveComponent* primitive = view.get<PrimitiveComponent*>(entity);
		if (!primitive)
			continue;

		if (!primitive->IsBodyCreated() || primitive->GetBodyHandle() == 0)
			continue;

		Actor* owner = primitive->GetOwner();
		if (!owner || owner->IsPendingDestroy())
		{
			DestroyBodyForComponent(*primitive);
			continue;
		}

		const JPH::BodyID bodyID(primitive->GetBodyHandle());
		if (primitive->BodyType == ERBBodyType::Dynamic)
		{
			// Write dynamic simulation results back to the owning primitive in world space.
			primitive->SetWorldPosition(ToRebel(bodies.GetPosition(bodyID)));
			primitive->SetWorldRotationQuat(ToRebel(bodies.GetRotation(bodyID)));
		}
	}

	EditorDebugDraw(scene);
}

bool PhysicsSystem::LineTraceSingle(const Vector3& start, const Vector3& end, TraceHit& outHit, const TraceQueryParams& params) const
{
	outHit = {};
	if (m_Impl == nullptr)
		return false;

	const Vector3 displacement = end - start;
	const float distance = glm::length(displacement);
	if (distance <= kSmallTraceDistance)
		return false;

	TraceObjectLayerFilter objectFilter(params.Channel);
	TraceBodyFilter bodyFilter(params);

	const JPH::RRayCast ray(ToJoltR(start), ToJolt(displacement));
	JPH::RayCastResult rayHit;
	if (!m_Impl->System.GetNarrowPhaseQuery().CastRay(ray, rayHit, {}, objectFilter, bodyFilter))
		return false;

	return FillTraceHitFromRay(m_Impl->System, ray, distance, rayHit, outHit);
}

bool PhysicsSystem::LineTraceMulti(const Vector3& start, const Vector3& end, std::vector<TraceHit>& outHits, const TraceQueryParams& params) const
{
	outHits.clear();
	if (m_Impl == nullptr)
		return false;

	const Vector3 displacement = end - start;
	const float distance = glm::length(displacement);
	if (distance <= kSmallTraceDistance)
		return false;

	TraceObjectLayerFilter objectFilter(params.Channel);
	TraceBodyFilter bodyFilter(params);

	const JPH::RRayCast ray(ToJoltR(start), ToJolt(displacement));
	JPH::RayCastSettings settings;
	JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

	m_Impl->System.GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, objectFilter, bodyFilter);
	if (!collector.HadHit())
		return false;

	collector.Sort();
	outHits.reserve(collector.mHits.size());

	for (const JPH::RayCastResult& joltHit : collector.mHits)
	{
		TraceHit hit;
		if (FillTraceHitFromRay(m_Impl->System, ray, distance, joltHit, hit))
			outHits.push_back(hit);
	}

	return !outHits.empty();
}

bool PhysicsSystem::SphereTraceSingle(const Vector3& start, const Vector3& end, float radius, TraceHit& outHit, const TraceQueryParams& params) const
{
	outHit = {};
	if (m_Impl == nullptr || radius <= 0.0f)
		return false;

	const Vector3 displacement = end - start;
	const JPH::SphereShape sphere(radius);
	return CastShapeSingleInternal(m_Impl->System, sphere, start, Quaternion(1.0f, 0.0f, 0.0f, 0.0f), displacement, params, outHit);
}

bool PhysicsSystem::CapsuleTraceSingle(const Vector3& start, const Vector3& end, float halfHeight, float radius, TraceHit& outHit, const TraceQueryParams& params) const
{
	outHit = {};
	if (m_Impl == nullptr || radius <= 0.0f || halfHeight <= 0.0f)
		return false;

	if (halfHeight <= radius + kSmallTraceDistance)
		return SphereTraceSingle(start, end, radius, outHit, params);

	const Vector3 displacement = end - start;
	const float capsuleHalfHeight = std::max(halfHeight - radius, 0.001f);

	JPH::CapsuleShapeSettings capsuleSettings(capsuleHalfHeight, radius);
	auto capsuleResult = capsuleSettings.Create();
	if (capsuleResult.HasError())
		return false;

	const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::DegreesToRadians(90.0f));
	JPH::RotatedTranslatedShapeSettings orientedCapsuleSettings(JPH::Vec3::sZero(), rotation, capsuleResult.Get());
	auto orientedCapsuleResult = orientedCapsuleSettings.Create();
	if (orientedCapsuleResult.HasError())
		return false;

	JPH::Ref<JPH::Shape> orientedCapsule = orientedCapsuleResult.Get();

	return CastShapeSingleInternal(
		m_Impl->System,
		*orientedCapsule,
		start,
		Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
		displacement,
		params,
		outHit);
}

bool PhysicsSystem::BoxTraceSingle(
	const Vector3& start,
	const Vector3& end,
	const Vector3& halfExtents,
	const Quaternion& rotation,
	TraceHit& outHit,
	const TraceQueryParams& params) const
{
	outHit = {};
	if (m_Impl == nullptr || halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
		return false;

	const Vector3 displacement = end - start;
	const JPH::BoxShape box(ToJolt(halfExtents));
	return CastShapeSingleInternal(m_Impl->System, box, start, rotation, displacement, params, outHit);
}




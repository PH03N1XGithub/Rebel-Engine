#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Components/Components.h"
#include "Engine/Physics/PhysicsSystem.h"

namespace
{
	constexpr float kFixedDt = 1.0f / 60.0f;

	bool NearlyEqual(const Vector3& a, const Vector3& b, float eps = 1e-4f)
	{
		return std::abs(a.x - b.x) <= eps
			&& std::abs(a.y - b.y) <= eps
			&& std::abs(a.z - b.z) <= eps;
	}
}

TEST_CASE("Physics: Child Primitive Does Not Move Root", "[physics][hierarchy]")
{
	Scene scene;
	PhysicsSystem physics;
	physics.Init();
	physics.SetFixedDeltaTime(kFixedDt);

	Actor& actor = scene.SpawnActor<Actor>();
	auto& root = actor.GetComponent<SceneComponent>();
	root.SetWorldPosition(Vector3(0.0f, 0.0f, 0.0f));

	auto& primitive = actor.AddObjectComponent<BoxComponent>();
	primitive.BodyType = ERBBodyType::Dynamic;
	primitive.ObjectChannel = CollisionChannel::WorldDynamic;
	primitive.HalfExtent = Vector3(0.5f, 0.5f, 0.5f);
	primitive.SetPosition(Vector3(100.0f, 0.0f, 0.0f)); // local offset from root

	const Vector3 initialRootWorld = root.GetWorldPosition();
	const Vector3 initialPrimitiveWorld = primitive.GetWorldPosition();

	REQUIRE(NearlyEqual(initialRootWorld, Vector3(0.0f, 0.0f, 0.0f)));
	REQUIRE(NearlyEqual(initialPrimitiveWorld, Vector3(100.0f, 0.0f, 0.0f)));

	for (int i = 0; i < 5; ++i)
	{
		physics.Step(scene, kFixedDt);
	}

	const Vector3 finalRootWorld = root.GetWorldPosition();
	const Vector3 finalPrimitiveWorld = primitive.GetWorldPosition();

	CHECK(NearlyEqual(finalRootWorld, initialRootWorld));
	CHECK(!NearlyEqual(finalPrimitiveWorld, initialPrimitiveWorld));
	CHECK(!NearlyEqual(finalPrimitiveWorld, Vector3(100.0f, 0.0f, 0.0f)));
	CHECK(!NearlyEqual(finalPrimitiveWorld, finalRootWorld));
	REQUIRE(NearlyEqual(finalRootWorld, initialRootWorld));

	physics.Shutdown();
}

TEST_CASE("Physics: Root Primitive Moves Actor", "[physics][hierarchy]")
{
	Scene scene;
	PhysicsSystem physics;
	physics.Init();
	physics.SetFixedDeltaTime(kFixedDt);

	Actor& actor = scene.SpawnActor<Actor>();

	auto& rootPrimitive = actor.AddObjectComponent<BoxComponent>();
	rootPrimitive.BodyType = ERBBodyType::Dynamic;
	rootPrimitive.ObjectChannel = CollisionChannel::WorldDynamic;
	rootPrimitive.HalfExtent = Vector3(0.5f, 0.5f, 0.5f);

	actor.SetRootComponent(&rootPrimitive);
	rootPrimitive.SetWorldPosition(Vector3(0.0f, 0.0f, 0.0f));

	const Vector3 initialRootWorld = rootPrimitive.GetWorldPosition();
	REQUIRE(NearlyEqual(initialRootWorld, Vector3(0.0f, 0.0f, 0.0f)));

	for (int i = 0; i < 5; ++i)
	{
		physics.Step(scene, kFixedDt);
	}

	const Vector3 finalRootWorld = rootPrimitive.GetWorldPosition();

	CHECK(!NearlyEqual(finalRootWorld, initialRootWorld));
	REQUIRE(!NearlyEqual(finalRootWorld, Vector3(0.0f, 0.0f, 0.0f)));

	physics.Shutdown();
}


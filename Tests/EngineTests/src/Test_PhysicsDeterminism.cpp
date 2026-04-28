#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Components/Components.h"
#include "Engine/Physics/PhysicsSystem.h"

#include <vector>

namespace
{
    Vector3 RunFallingSphereSimulation(const std::vector<float>& frameDeltas)
    {
        Scene scene;
        PhysicsSystem physics;
        physics.Init();
        physics.SetFixedDeltaTime(1.0f / 60.0f);
        physics.SetMaxSubsteps(8);

        Actor& actor = scene.SpawnActor<Actor>();
        auto& root = actor.GetComponent<SceneComponent>();
        root.SetPosition(Vector3(0.0f, 0.0f, 10.0f));

        auto& sphere = actor.AddObjectComponent<SphereComponent>();
        sphere.BodyType = ERBBodyType::Dynamic;
        sphere.ObjectChannel = CollisionChannel::WorldDynamic;
        sphere.Radius = 0.5f;

        for (float dt : frameDeltas)
        {
            physics.Step(scene, dt);
        }

        const Vector3 finalPosition = root.GetPosition();
        physics.Shutdown();
        return finalPosition;
    }
}

TEST_CASE("Physics trace miss returns safe null result", "[engine][physics][identity][negative]")
{
    Scene scene;
    PhysicsSystem physics;
    physics.Init();

    TraceHit hit{};
    hit.bBlockingHit = true;
    hit.Distance = 123.0f;
    hit.HitEntity = static_cast<EntityID>(1);

    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;

    const bool didHit = physics.LineTraceSingle(
        Vector3(1000.0f, 1000.0f, 1000.0f),
        Vector3(1005.0f, 1000.0f, 1000.0f),
        hit,
        params);

    REQUIRE(!didHit);
    REQUIRE(!hit.bBlockingHit);
    REQUIRE(hit.HitEntity == entt::null);
    REQUIRE(hit.Distance == Catch::Approx(0.0f).margin(1e-6f));

    physics.Shutdown();
}

TEST_CASE("Destroyed hit actor resolves to nullptr safely", "[engine][physics][identity][lifecycle]")
{
    Scene scene;
    PhysicsSystem physics;
    physics.Init();

    Actor& actor = scene.SpawnActor<Actor>();
    auto& root = actor.GetComponent<SceneComponent>();
    root.SetPosition(Vector3(0.0f, 0.0f, 0.0f));

    auto& box = actor.AddObjectComponent<BoxComponent>();
    box.BodyType = ERBBodyType::Static;
    box.ObjectChannel = CollisionChannel::WorldStatic;
    box.HalfExtent = Vector3(0.5f, 0.5f, 0.5f);

    physics.Step(scene, 1.0f / 60.0f);

    TraceHit hit{};
    TraceQueryParams params{};
    params.Channel = CollisionChannel::Any;

    const bool didHit = physics.LineTraceSingle(
        Vector3(-5.0f, 0.0f, 0.0f),
        Vector3(5.0f, 0.0f, 0.0f),
        hit,
        params);

    REQUIRE(didHit);
    REQUIRE(hit.HitEntity != entt::null);

    actor.Destroy();
    scene.FlushPendingActorDestroy();

    Actor* resolved = scene.GetActor(hit.HitEntity);
    REQUIRE(resolved == nullptr);

    physics.Shutdown();
}

TEST_CASE("Fixed-step integration is deterministic across delta partitioning", "[engine][physics][determinism][fixed-step]")
{
    const float fixedDeltaTime = 1.0f / 60.0f;

    std::vector<float> fineDeltas(120, fixedDeltaTime);
    std::vector<float> coarseDeltas(20, 6.0f * fixedDeltaTime);

    const Vector3 finePosition = RunFallingSphereSimulation(fineDeltas);
    const Vector3 coarsePosition = RunFallingSphereSimulation(coarseDeltas);

    REQUIRE(finePosition.x == Catch::Approx(coarsePosition.x).margin(1e-3f));
    REQUIRE(finePosition.y == Catch::Approx(coarsePosition.y).margin(1e-3f));
    REQUIRE(finePosition.z == Catch::Approx(coarsePosition.z).margin(1e-3f));
}


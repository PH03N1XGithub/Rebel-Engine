#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Components/Components.h"
#include "Engine/Physics/PhysicsSystem.h"

TEST_CASE("Physics hit to Actor resolution test", "[engine][physics][identity]")
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

    Actor* resolved = scene.GetActor(hit.HitEntity);
    REQUIRE(resolved != nullptr);
    REQUIRE((*resolved == actor));

    physics.Shutdown();
}

TEST_CASE("Fixed timestep clamp test", "[engine][physics][timestep]")
{
    Scene scene;
    PhysicsSystem physics;
    physics.Init();

    physics.SetFixedDeltaTime(1.0f / 60.0f);
    physics.SetMaxSubsteps(4);

    physics.Step(scene, 1.0f);

    REQUIRE(physics.GetLastSubstepCount() == 4u);
    REQUIRE(physics.GetMaxSubsteps() == 4u);

    physics.Shutdown();
}


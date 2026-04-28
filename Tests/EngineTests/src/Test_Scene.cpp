#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Components/Components.h"

TEST_CASE("Scene world transform stability", "[engine][scene][transform]")
{
    Scene scene;
    Actor& actor = scene.SpawnActor<Actor>();

    auto& root = actor.GetComponent<SceneComponent>();
    root.SetPosition(Vector3(10.0f, 0.0f, 0.0f));

    auto& child = actor.AddObjectComponent<SceneComponent>();
    child.SetPosition(Vector3(0.0f, 3.0f, 0.0f));

    scene.UpdateTransforms();

    const Mat4 a = child.GetWorldTransform();
    const Mat4 b = child.GetWorldTransform();

    REQUIRE(a[3].x == Catch::Approx(b[3].x).margin(1e-6f));
    REQUIRE(a[3].y == Catch::Approx(b[3].y).margin(1e-6f));
    REQUIRE(a[3].z == Catch::Approx(b[3].z).margin(1e-6f));
}


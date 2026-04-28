#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"

TEST_CASE("Spawn + Destroy Actor lifecycle", "[engine][scene][lifecycle]")
{
    Scene scene;

    Actor& actor = scene.SpawnActor<Actor>();
    REQUIRE(actor.IsValid());
    REQUIRE(scene.GetActors().Num() == 1);

    scene.BeginPlay();
    REQUIRE(actor.HasBegunPlay());

    actor.Destroy();
    scene.FlushPendingActorDestroy();

    REQUIRE(scene.GetActors().Num() == 0);
}


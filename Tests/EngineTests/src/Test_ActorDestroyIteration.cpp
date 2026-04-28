#include "catch_amalgamated.hpp"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Actor.h"

class SelfDestroyActor : public Actor
{
    REFLECTABLE_CLASS(SelfDestroyActor, Actor)

public:
    static int32 TickCalls;

    void Tick(float dt) override
    {
        (void)dt;
        ++TickCalls;
        Destroy();
    }
};

int32 SelfDestroyActor::TickCalls = 0;

REFLECT_CLASS(SelfDestroyActor, Actor)
END_REFLECT_CLASS(SelfDestroyActor)

TEST_CASE("Destroying actor during tick iteration is safe", "[engine][scene][lifecycle][destroy-during-iteration]")
{
    SelfDestroyActor::TickCalls = 0;

    Scene scene;
    Actor& doomed = scene.SpawnActor<SelfDestroyActor>();
    Actor& survivor = scene.SpawnActor<Actor>();

    const entt::entity doomedHandle = doomed.GetHandle();
    const entt::entity survivorHandle = survivor.GetHandle();

    scene.BeginPlay();
    scene.PrepareTick();
    scene.TickGroup(ActorTickGroup::PrePhysics, 1.0f / 60.0f);
    scene.TickGroup(ActorTickGroup::PostPhysics, 1.0f / 60.0f);
    scene.TickGroup(ActorTickGroup::PostUpdate, 1.0f / 60.0f);
    scene.FinalizeTick();

    REQUIRE(SelfDestroyActor::TickCalls == 1);
    REQUIRE(scene.GetActor(doomedHandle) == nullptr);
    REQUIRE(scene.GetActor(survivorHandle) != nullptr);
    REQUIRE(scene.GetActors().Num() == 1);
}


#include "catch_amalgamated.hpp"

#include "Engine/Gameplay/Framework/Pawn.h"

TEST_CASE("Pawn movement intent buffering and consume reset", "[engine][pawn][input][intent]")
{
    Pawn pawn;

    pawn.AddMovementInput(Vector3(1.0f, 0.0f, 0.0f));
    pawn.AddMovementInput(Vector3(0.0f, 1.0f, 0.0f));

    const Vector3 pendingBeforeConsume = pawn.GetPendingMovementInput();
    REQUIRE(pendingBeforeConsume.x == Catch::Approx(1.0f));
    REQUIRE(pendingBeforeConsume.y == Catch::Approx(1.0f));
    REQUIRE(pendingBeforeConsume.z == Catch::Approx(0.0f));

    const Vector3 consumed = pawn.ConsumeMovementInput();
    REQUIRE(consumed.x == Catch::Approx(1.0f));
    REQUIRE(consumed.y == Catch::Approx(1.0f));
    REQUIRE(consumed.z == Catch::Approx(0.0f));

    const Vector3 pendingAfterConsume = pawn.GetPendingMovementInput();
    REQUIRE(pendingAfterConsume.x == Catch::Approx(0.0f));
    REQUIRE(pendingAfterConsume.y == Catch::Approx(0.0f));
    REQUIRE(pendingAfterConsume.z == Catch::Approx(0.0f));
}


#include "catch_amalgamated.hpp"
#include "Engine/Input/InputFrame.h"

namespace
{
    KeyState EvaluateKeyStateTransition(const bool prevDown, const bool nowDown)
    {
        KeyState state{};
        state.Down = nowDown;
        state.PressedThisFrame = !prevDown && nowDown;
        state.ReleasedThisFrame = prevDown && !nowDown;
        return state;
    }
}

TEST_CASE("KeyState transition: PrevDown=false NowDown=false", "[engine][input][keystate]")
{
    const KeyState state = EvaluateKeyStateTransition(false, false);

    REQUIRE(state.PressedThisFrame == false);
    REQUIRE(state.ReleasedThisFrame == false);
    REQUIRE((state.PressedThisFrame && state.ReleasedThisFrame) == false);
}

TEST_CASE("KeyState transition: PrevDown=false NowDown=true", "[engine][input][keystate]")
{
    const KeyState state = EvaluateKeyStateTransition(false, true);

    REQUIRE(state.PressedThisFrame == true);
    REQUIRE(state.ReleasedThisFrame == false);
    REQUIRE((state.PressedThisFrame && state.ReleasedThisFrame) == false);
}

TEST_CASE("KeyState transition: PrevDown=true NowDown=false", "[engine][input][keystate]")
{
    const KeyState state = EvaluateKeyStateTransition(true, false);

    REQUIRE(state.PressedThisFrame == false);
    REQUIRE(state.ReleasedThisFrame == true);
    REQUIRE((state.PressedThisFrame && state.ReleasedThisFrame) == false);
}

TEST_CASE("KeyState transition: PrevDown=true NowDown=true", "[engine][input][keystate]")
{
    const KeyState state = EvaluateKeyStateTransition(true, true);

    REQUIRE(state.PressedThisFrame == false);
    REQUIRE(state.ReleasedThisFrame == false);
    REQUIRE((state.PressedThisFrame && state.ReleasedThisFrame) == false);
}


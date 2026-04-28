#include "catch_amalgamated.hpp"

#include "Engine/Input/InputModule.h"
#include "Engine/Input/PlayerInput.h"
#include "Engine/Framework/Window.h"

namespace
{
    void SetKeyState(InputModule& inputModule, const uint16 key, const bool bDown)
    {
        Event event{};
        event.Type = bDown ? Event::Type::KeyPressed : Event::Type::KeyReleased;
        event.Key = key;
        inputModule.OnEvent(event);
    }
}

TEST_CASE("MoveForward axis accumulation from W/S bindings", "[engine][input][axis]")
{
    InputModule::s_Instance = nullptr;

    InputModule inputModule;
    inputModule.Init();

    PlayerInput playerInput;
    uint64 frameId = 1;
    double timeSeconds = 0.0;

    // 1) W only -> +1
    SetKeyState(inputModule, GLFW_KEY_W, true);
    SetKeyState(inputModule, GLFW_KEY_S, false);
    playerInput.EvaluateFrame(frameId++, timeSeconds += 0.016);
    REQUIRE(playerInput.GetAxis(InputAction::MoveForward) == Catch::Approx(1.0f));

    // 2) S only -> -1
    SetKeyState(inputModule, GLFW_KEY_W, false);
    SetKeyState(inputModule, GLFW_KEY_S, true);
    playerInput.EvaluateFrame(frameId++, timeSeconds += 0.016);
    REQUIRE(playerInput.GetAxis(InputAction::MoveForward) == Catch::Approx(-1.0f));

    // 3) W + S -> 0
    SetKeyState(inputModule, GLFW_KEY_W, true);
    SetKeyState(inputModule, GLFW_KEY_S, true);
    playerInput.EvaluateFrame(frameId++, timeSeconds += 0.016);
    REQUIRE(playerInput.GetAxis(InputAction::MoveForward) == Catch::Approx(0.0f));

    // 4) None -> 0
    SetKeyState(inputModule, GLFW_KEY_W, false);
    SetKeyState(inputModule, GLFW_KEY_S, false);
    playerInput.EvaluateFrame(frameId++, timeSeconds += 0.016);
    REQUIRE(playerInput.GetAxis(InputAction::MoveForward) == Catch::Approx(0.0f));
}


#include "catch_amalgamated.hpp"

#include "Engine/Input/InputComponent.h"
#include "Engine/Input/InputModule.h"
#include "Engine/Input/InputStack.h"
#include "Engine/Input/PlayerInput.h"
#include "Engine/Framework/Window.h"

namespace
{
    struct JumpMarker
    {
        bool* bExecuted = nullptr;

        void Mark()
        {
            if (bExecuted)
                *bExecuted = true;
        }
    };

    void SetKeyState(InputModule& inputModule, const uint16 key, const bool bDown)
    {
        Event event{};
        event.Type = bDown ? Event::Type::KeyPressed : Event::Type::KeyReleased;
        event.Key = key;
        inputModule.OnEvent(event);
    }
}

TEST_CASE("Input stack executes only higher priority Jump binding when consumed", "[engine][input][stack][consume]")
{
    InputModule::s_Instance = nullptr;

    InputModule inputModule;
    inputModule.Init();

    PlayerInput playerInput;

    // Generate Jump PressedThisFrame = true
    SetKeyState(inputModule, GLFW_KEY_SPACE, true);
    playerInput.EvaluateFrame(1, 0.016);

    bool bHighPriorityExecuted = false;
    bool bLowPriorityExecuted = false;
    JumpMarker highMarker{&bHighPriorityExecuted};
    JumpMarker lowMarker{&bLowPriorityExecuted};

    InputComponent highPriorityComponent;
    InputComponent lowPriorityComponent;

    // Higher priority consumes Jump
    highPriorityComponent.AddActionBinding(InputAction::Jump, InputEventType::Pressed, true).BindRaw(&highMarker, &JumpMarker::Mark);
    lowPriorityComponent.AddActionBinding(InputAction::Jump, InputEventType::Pressed, false).BindRaw(&lowMarker, &JumpMarker::Mark);

    InputStack inputStack;
    inputStack.Push(&lowPriorityComponent, 0, false);
    inputStack.Push(&highPriorityComponent, 100, false);

    inputStack.Dispatch(playerInput);

    REQUIRE(bHighPriorityExecuted);
    REQUIRE(!bLowPriorityExecuted);
}


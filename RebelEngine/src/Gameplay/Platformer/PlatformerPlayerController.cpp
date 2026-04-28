#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Platformer/PlatformerPlayerController.h"

#include "Engine/Gameplay/Platformer/PlatformerCharacter.h"
#include "Engine/Input/InputFrame.h"
#include "Engine/Gameplay/Framework/Pawn.h"

DEFINE_LOG_CATEGORY(testInputHarnessLog)

PlatformerPlayerController::PlatformerPlayerController()
{
    SetTickPriority(-200);
}

void PlatformerPlayerController::SetupInputComponent()
{
    if (PlayerInput* playerInput = GetPlayerInput())
        playerInput->SetDebugLoggingEnabled(true);

    rebuildBindings();
    SetInputSetupComplete(true);
}

void PlatformerPlayerController::ProcessPlayerInput(float dt)
{
    (void)dt;

    PlayerInput* playerInput = GetPlayerInput();
    if (!playerInput)
        return;

    InputStack& inputStack = GetInputStack();
    inputStack.Clear();

    inputStack.Push(
        GetPrimaryInputComponent(),
        0,
        false);

    inputStack.Dispatch(*playerInput);
}

void PlatformerPlayerController::rebuildBindings()
{
    InputComponent* highInput = GetPrimaryInputComponent();

    highInput->ClearBindings();

    highInput->AddAxisBinding(InputAction::MoveForward, false).BindRaw(this, &PlatformerPlayerController::OnHighMoveForward);
    highInput->AddAxisBinding(InputAction::MoveRight, false).BindRaw(this, &PlatformerPlayerController::OnHighMoveRight);
    highInput->AddActionBinding(InputAction::Jump, InputEventType::Pressed, false).BindRaw(this, &PlatformerPlayerController::OnHighJumpPressed);
    highInput->AddActionBinding(InputAction::Sprint, InputEventType::Pressed, false).BindRaw(this, &PlatformerPlayerController::OnDashPressed);
}

void PlatformerPlayerController::OnHighMoveForward(const float value)
{
    Vector3 direction = GetActorForwardVector();
    if (Pawn* pawn = GetPawn())
        pawn->AddMovementInput(direction, value);
}

void PlatformerPlayerController::OnHighMoveRight(const float value)
{
    Vector3 direction = GetActorRightVector();
    if (Pawn* pawn = GetPawn())
        pawn->AddMovementInput(direction, value);
}

void PlatformerPlayerController::OnHighJumpPressed()
{
    RB_LOG(testInputHarnessLog, info, "HighPriority Jump Fired")
    if (PlatformerCharacter* platformerCharacter = dynamic_cast<PlatformerCharacter*>(GetPawn()))
    {
        platformerCharacter->HandleJumpPressed();
        return;
    }

    if (Pawn* pawn = GetPawn())
        pawn->Jump();
}

void PlatformerPlayerController::OnDashPressed()
{
    if (PlatformerCharacter* platformerCharacter = dynamic_cast<PlatformerCharacter*>(GetPawn()))
    {
        if (platformerCharacter->TryDash())
            RB_LOG(testInputHarnessLog, info, "Dash triggered")
    }
}

void PlatformerPlayerController::OnLowMoveForward(const float value)
{
    RB_LOG(testInputHarnessLog, info, "LowPriority Axis MoveForward = {:.2f}", value)
    if (Pawn* pawn = GetPawn())
        pawn->AddMovementInput(pawn->GetActorForwardVector(), value);
}

void PlatformerPlayerController::OnLowMoveRight(const float value)
{
    RB_LOG(testInputHarnessLog, info, "LowPriority Axis MoveRight = {:.2f}", value)
    if (Pawn* pawn = GetPawn())
        pawn->AddMovementInput(pawn->GetActorRightVector(), value);
}

void PlatformerPlayerController::OnLowJumpPressed()
{
    RB_LOG(testInputHarnessLog, info, "LowPriority Jump Fired")
    if (Pawn* pawn = GetPawn())
        pawn->Jump();
}

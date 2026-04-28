#pragma once

#include "Engine/Gameplay/Framework/PlayerController.h"
#include "Engine/Input/InputComponent.h"

class PlatformerPlayerController : public PlayerController
{
    REFLECTABLE_CLASS(PlatformerPlayerController, PlayerController)
public:
    PlatformerPlayerController();
    ~PlatformerPlayerController() override = default;

    void SetupInputComponent() override;
    void ProcessPlayerInput(float dt) override;

private:
    void rebuildBindings();
    void OnHighMoveForward(float value);
    void OnHighMoveRight(float value);
    void OnHighJumpPressed();
    void OnDashPressed();
    void OnLowMoveForward(float value);
    void OnLowMoveRight(float value);
    void OnLowJumpPressed();
    
};

REFLECT_CLASS(PlatformerPlayerController, PlayerController)
END_REFLECT_CLASS(PlatformerPlayerController)

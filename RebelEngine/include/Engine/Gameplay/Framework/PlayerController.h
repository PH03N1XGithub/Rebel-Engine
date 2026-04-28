#pragma once

#include <memory>

#include "Engine/Assets/TPrefabOf.h"
#include "Engine/Framework/TSubclassOf.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Gameplay/Framework/PlayerCameraManager.h"
#include "Engine/Input/InputComponent.h"
#include "Engine/Input/InputStack.h"
#include "Engine/Input/PlayerInput.h"

class PlayerController : public Controller
{
    REFLECTABLE_CLASS(PlayerController, Controller)
public:
    PlayerController();
    ~PlayerController() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void Tick(float dt) override;
    void Possess(Pawn* pawn) override;
    void UnPossess() override;

    virtual void SetupInputComponent();
    virtual void ProcessPlayerInput(float dt);
    void PreSimulationInputUpdate(uint64 frameId, float dt);
    class PlayerCameraManager* GetPlayerCameraManager() const { return m_PlayerCameraManager; }

protected:
    PlayerInput* GetPlayerInput() const { return m_PlayerInput.get(); }
    InputComponent* GetPrimaryInputComponent() const { return m_PrimaryInputComponent.get(); }
    InputStack& GetInputStack() { return m_InputStack; }
    const InputStack& GetInputStack() const { return m_InputStack; }
    void SetInputSetupComplete(bool bComplete) { m_bInputSetupComplete = bComplete; }
    bool IsInputSetupComplete() const { return m_bInputSetupComplete; }
    void EnsurePlayerCameraManager();
    void UpdateControlRotationFromInput(float dt);

private:
    class PlayerCameraManager* m_PlayerCameraManager = nullptr;
    std::unique_ptr<PlayerInput> m_PlayerInput;
    std::unique_ptr<InputComponent> m_PrimaryInputComponent;
    InputStack m_InputStack;

    bool m_bInputSetupComplete = false;
    double m_InputTimeSeconds = 0.0;

    float m_MoveForwardValue = 0.0f;
    float m_MoveRightValue = 0.0f;
    float m_LookYawValue = 0.0f;
    float m_LookPitchValue = 0.0f;
    float m_LookYawRate = 180.0f;
    float m_LookPitchRate = 120.0f;
    float m_MinViewPitch = -80.0f;
    float m_MaxViewPitch = 80.0f;
    Rebel::TPrefabOf<PlayerCameraManager> m_PlayerCameraManagerPrefab;
    Rebel::TSubclassOf<PlayerCameraManager> m_PlayerCameraManagerClass;
};

REFLECT_CLASS(PlayerController, Controller)
    REFLECT_PROPERTY(PlayerController, m_LookYawRate,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerController, m_LookPitchRate,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerController, m_MinViewPitch,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerController, m_MaxViewPitch,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerController, m_PlayerCameraManagerPrefab,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerController, m_PlayerCameraManagerClass,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(PlayerController)


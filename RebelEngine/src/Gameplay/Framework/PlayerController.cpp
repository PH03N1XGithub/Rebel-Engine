#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/PlayerController.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Gameplay/Framework/PlayerCameraManager.h"
#include "Engine/Scene/World.h"

namespace
{
    float NormalizeDegrees(float angleDegrees)
    {
        float normalized = std::fmod(angleDegrees, 360.0f);
        if (normalized > 180.0f)
            normalized -= 360.0f;
        else if (normalized < -180.0f)
            normalized += 360.0f;

        return normalized;
    }
}

PlayerController::PlayerController()
    : m_PlayerInput(std::make_unique<PlayerInput>())
    , m_PrimaryInputComponent(std::make_unique<InputComponent>())
{
    SetTickGroup(ActorTickGroup::PrePhysics);
    SetTickPriority(-100);
    SetControlRotation(Vector3(-10.0f, 0.0f, 0.0f));
}

void PlayerController::BeginPlay()
{
    Controller::BeginPlay();

    if (!m_bInputSetupComplete)
        SetupInputComponent();

    EnsurePlayerCameraManager();
    if (m_PlayerCameraManager && GetPawn())
        m_PlayerCameraManager->SetTarget(GetPawn());
}

void PlayerController::EndPlay()
{
    if (m_PlayerCameraManager && !m_PlayerCameraManager->IsPendingDestroy())
        m_PlayerCameraManager->Destroy();

    m_PlayerCameraManager = nullptr;
    Controller::EndPlay();
}

void PlayerController::Tick(const float dt)
{
    if (GEngine && !GEngine->ShouldProcessGameplayInput())
    {
        Controller::Tick(dt);
        return;
    }

    UpdateControlRotationFromInput(dt);
    ProcessPlayerInput(dt);
    Controller::Tick(dt);
}

void PlayerController::Possess(Pawn* pawn)
{
    Controller::Possess(pawn);

    EnsurePlayerCameraManager();
    if (m_PlayerCameraManager)
        m_PlayerCameraManager->SetTarget(GetPawn());
}

void PlayerController::UnPossess()
{
    Controller::UnPossess();

    if (m_PlayerCameraManager)
        m_PlayerCameraManager->ClearTarget();
}

void PlayerController::PreSimulationInputUpdate(const uint64 frameId, const float dt)
{
    if (GEngine && !GEngine->ShouldProcessGameplayInput())
        return;

    if (!m_PlayerInput)
        return;

    m_InputTimeSeconds += dt;
    m_PlayerInput->EvaluateFrame(frameId, m_InputTimeSeconds);
}

void PlayerController::SetupInputComponent()
{
    if (!m_PrimaryInputComponent)
        return;

    m_PrimaryInputComponent->ClearBindings();

    m_InputStack.Clear();
    m_InputStack.Push(m_PrimaryInputComponent.get(), 0, false);

    m_bInputSetupComplete = true;
}

void PlayerController::ProcessPlayerInput(float dt)
{
    (void)dt;

    if (!m_PlayerInput)
        return;

    m_InputStack.Dispatch(*m_PlayerInput);
}

void PlayerController::EnsurePlayerCameraManager()
{
    if (m_PlayerCameraManager && !m_PlayerCameraManager->IsPendingDestroy())
        return;

    World* world = GetWorld();
    if (!world)
        return;

    m_PlayerCameraManager = nullptr;
    if (m_PlayerCameraManagerPrefab.GetHandle() != AssetHandle(0))
    {
        if (auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>())
        {
            if (PrefabAsset* prefab = dynamic_cast<PrefabAsset*>(assetModule->GetManager().Load(m_PlayerCameraManagerPrefab.GetHandle())))
            {
                const Rebel::Core::Reflection::TypeInfo* prefabActorType = nullptr;
                if (prefab->m_ActorTypeName.length() > 0)
                    prefabActorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(prefab->m_ActorTypeName);

                if (prefabActorType && prefabActorType->IsA(PlayerCameraManager::StaticType()))
                    m_PlayerCameraManager = static_cast<PlayerCameraManager*>(world->GetScene()->SpawnActorFromPrefab(*prefab));
            }
        }
    }

    if (!m_PlayerCameraManager)
    {
        const Rebel::Core::Reflection::TypeInfo* pcmType = m_PlayerCameraManagerClass.Get();
        if (!pcmType || !pcmType->IsA(PlayerCameraManager::StaticType()))
            pcmType = PlayerCameraManager::StaticType();

        m_PlayerCameraManager = static_cast<PlayerCameraManager*>(&world->GetScene()->SpawnActor(pcmType));
    }

    if (!m_PlayerCameraManager)
        return;

    m_PlayerCameraManager->SetOwningPlayerController(this);
    if (GetPawn())
        m_PlayerCameraManager->SetTarget(GetPawn());
}
DEFINE_LOG_CATEGORY(PlayerControllerLOG);
void PlayerController::UpdateControlRotationFromInput(const float dt)
{
    if (!m_PlayerInput || dt <= 0.0f)
        return;

    m_LookYawValue = m_PlayerInput->GetAxis(InputAction::LookYaw);
    m_LookPitchValue = m_PlayerInput->GetAxis(InputAction::LookPitch);
    RB_LOG( PlayerControllerLOG,trace," LookYawValue {}",m_LookYawValue)
    Vector3 controlRotation = GetControlRotation();
    controlRotation.z = NormalizeDegrees(controlRotation.z + m_LookYawValue * m_LookYawRate * dt);
    controlRotation.x = glm::clamp(controlRotation.x + m_LookPitchValue * m_LookPitchRate * dt, m_MinViewPitch, m_MaxViewPitch);
    controlRotation.y = 0.0f;
    SetControlRotation(controlRotation);
    SetActorRotation(controlRotation);
}

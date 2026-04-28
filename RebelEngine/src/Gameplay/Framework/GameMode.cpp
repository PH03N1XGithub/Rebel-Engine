#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/GameMode.h"

#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Platformer/PlatformerPlayerController.h"
#include "Engine/Gameplay/Platformer/PlatformerCharacter.h"
#include "Engine/Gameplay/Framework/LocomotionCharacter.h"
#include "Engine/Scene/World.h"

void GameMode::StartPlay(World& world)
{
    Pawn* pawn = nullptr;
    if (m_DefaultPawnPrefab.GetHandle() != AssetHandle(0))
    {
        if (auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>())
        {
            if (PrefabAsset* prefab = dynamic_cast<PrefabAsset*>(assetModule->GetManager().Load(m_DefaultPawnPrefab.GetHandle())))
            {
                const Rebel::Core::Reflection::TypeInfo* prefabActorType = nullptr;
                if (prefab->m_ActorTypeName.length() > 0)
                    prefabActorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(prefab->m_ActorTypeName);

                if (prefabActorType && prefabActorType->IsA(Pawn::StaticType()))
                    pawn = static_cast<Pawn*>(world.GetScene()->SpawnActorFromPrefab(*prefab));
            }
        }
    }

    if (!pawn)
    {
        const Rebel::Core::Reflection::TypeInfo* pawnType = m_DefaultPawnClass.Get();
        if (!pawnType || !pawnType->IsA(Pawn::StaticType()))
            pawnType = Pawn::StaticType();

        pawn = static_cast<Pawn*>(&world.GetScene()->SpawnActor(pawnType));
    }

    PlayerController* controller = nullptr;
    if (m_PlayerControllerPrefab.GetHandle() != AssetHandle(0))
    {
        if (auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>())
        {
            if (PrefabAsset* prefab = dynamic_cast<PrefabAsset*>(assetModule->GetManager().Load(m_PlayerControllerPrefab.GetHandle())))
            {
                const Rebel::Core::Reflection::TypeInfo* prefabActorType = nullptr;
                if (prefab->m_ActorTypeName.length() > 0)
                    prefabActorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(prefab->m_ActorTypeName);

                if (prefabActorType && prefabActorType->IsA(PlayerController::StaticType()))
                    controller = static_cast<PlayerController*>(world.GetScene()->SpawnActorFromPrefab(*prefab));
            }
        }
    }

    if (!controller)
    {
        const Rebel::Core::Reflection::TypeInfo* controllerType = m_PlayerControllerClass.Get();
        if (!controllerType || !controllerType->IsA(PlayerController::StaticType()))
            controllerType = PlayerController::StaticType();

        controller = static_cast<PlayerController*>(&world.GetScene()->SpawnActor(controllerType));
    }

    if (controller && pawn)
        controller->Possess(pawn);
}

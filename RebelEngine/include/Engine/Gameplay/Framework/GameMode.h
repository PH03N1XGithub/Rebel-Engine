#pragma once

#include "Character.h"
#include "PlayerController.h"
#include "Engine/Assets/TPrefabOf.h"
#include "Engine/Framework/Core.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Framework/TSubclassOf.h"

class World;

class REBELENGINE_API GameMode
{
    REFLECTABLE_CLASS(GameMode, void)
public:
    virtual ~GameMode() = default;
    virtual void StartPlay(World& world);

    Rebel::TPrefabOf<Pawn> m_DefaultPawnPrefab;
    Rebel::TSubclassOf<Pawn> m_DefaultPawnClass = Pawn::StaticType();
    Rebel::TPrefabOf<PlayerController> m_PlayerControllerPrefab;
    Rebel::TSubclassOf<PlayerController> m_PlayerControllerClass = PlayerController::StaticType();
};

REFLECT_CLASS(GameMode, void)
    REFLECT_PROPERTY(GameMode, m_DefaultPawnPrefab,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(GameMode, m_DefaultPawnClass,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(GameMode, m_PlayerControllerPrefab,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(GameMode, m_PlayerControllerClass,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(GameMode)

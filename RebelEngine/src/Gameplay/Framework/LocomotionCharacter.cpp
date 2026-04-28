#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/LocomotionCharacter.h"

#include "Engine/Gameplay/Framework/CharacterLocomotionComponent.h"

LocomotionCharacter::LocomotionCharacter()
{
    // Create after Character subobjects so this component ticks after CMC in Actor::TickComponents.
    m_LocomotionComponent = &CreateDefaultSubobject<CharacterLocomotionComponent>();
}

void LocomotionCharacter::BeginPlay()
{
    Character::BeginPlay();

    if (!m_LocomotionComponent)
        m_LocomotionComponent = GetObjectComponent<CharacterLocomotionComponent>();
}

void LocomotionCharacter::Tick(float dt)
{
    Character::Tick(dt);
}

const FLocomotionState& LocomotionCharacter::GetLocomotionState() const
{
    if (m_LocomotionComponent)
        return m_LocomotionComponent->GetLocomotionState();

    return m_DefaultLocomotionState;
}

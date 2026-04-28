#pragma once

#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Framework/LocomotionTypes.h"

class CharacterLocomotionComponent;

class LocomotionCharacter : public Character
{
    REFLECTABLE_CLASS(LocomotionCharacter, Character)

public:
    LocomotionCharacter();
    ~LocomotionCharacter() override = default;

    void BeginPlay() override;
    void Tick(float dt) override;

    CharacterLocomotionComponent* GetLocomotionComponent() const { return m_LocomotionComponent; }
    const FLocomotionState& GetLocomotionState() const;

private:
    CharacterLocomotionComponent* m_LocomotionComponent = nullptr;
    FLocomotionState m_DefaultLocomotionState{};
};

REFLECT_CLASS(LocomotionCharacter, Character)
END_REFLECT_CLASS(LocomotionCharacter)

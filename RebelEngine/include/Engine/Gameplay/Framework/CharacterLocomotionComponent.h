#pragma once

#include "Engine/Components/IdentityComponents.h"
#include "Engine/Gameplay/Framework/LocomotionTypes.h"

class Character;
class CharacterMovementComponent;

class CharacterLocomotionComponent : public ActorComponent
{
    REFLECTABLE_CLASS(CharacterLocomotionComponent, ActorComponent)

public:
    void BeginPlay() override;
    void Tick(float deltaTime) override;

    const FLocomotionState& GetLocomotionState() const { return m_State; }
    const FLocomotionState& GetPreviousLocomotionState() const { return m_PreviousState; }

private:
    static float NormalizeDegrees(float angleDegrees);
    static float ComputePlanarYawDegrees(const Vector3& direction);
    bool ComputeIsMoving(float horizontalSpeed) const;
    ELocomotionAction ClassifyAction() const;
    bool ShouldClassifyPivoting() const;
    bool ShouldClassifyStarting() const;
    bool ShouldClassifyStopping() const;
    void ResolveOwnerReferences();
    void UpdateLocomotionState();

private:
    Character* m_Character = nullptr;
    CharacterMovementComponent* m_CharacterMovement = nullptr;

    FLocomotionState m_State{};
    FLocomotionState m_PreviousState{};
    bool m_bHasPreviousState = false;
};

REFLECT_CLASS(CharacterLocomotionComponent, ActorComponent)
END_REFLECT_CLASS(CharacterLocomotionComponent)
REFLECT_ECS_COMPONENT(CharacterLocomotionComponent)

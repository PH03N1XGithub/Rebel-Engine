#pragma once

#include "Engine/Gameplay/Framework/Pawn.h"

class CapsuleComponent;
struct CameraAnchorComponent;
class SkeletalMeshComponent;
class CharacterMovementComponent;

class Character : public Pawn
{
    REFLECTABLE_CLASS(Character, Pawn)

public:
    Character();
    ~Character() override = default;

    void BeginPlay() override;
    void Tick(float dt) override;;
    void LaunchCharacter(const Vector3& launchVelocity, bool bXYOverride = false, bool bZOverride = false);

    CapsuleComponent* GetCapsuleComponent() const { return m_Capsule; }
    CameraAnchorComponent* GetCameraAnchorComponent() const { return m_CameraAnchor; }
    SkeletalMeshComponent* GetMeshComponent() const { return m_Mesh; }
    CharacterMovementComponent* GetCharacterMovementComponent() const { return m_CharacterMovement; }

private:
    CapsuleComponent* m_Capsule = nullptr;
    CameraAnchorComponent* m_CameraAnchor = nullptr;
    SkeletalMeshComponent* m_Mesh = nullptr;
    CharacterMovementComponent* m_CharacterMovement = nullptr;
};

REFLECT_CLASS(Character, Pawn)
END_REFLECT_CLASS(Character)


#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/Character.h"

#include "Engine/Animation/AnimInstance.h"
#include "Engine/Components/CameraAnchorComponent.h"
#include "Engine/Components/CapsuleComponent.h"
#include "Engine/Components/SkeletalMeshComponent.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"

Character::Character()
{
    m_Capsule = &CreateDefaultSubobject<CapsuleComponent>();
    m_CameraAnchor = &CreateDefaultSubobject<CameraAnchorComponent>();
    m_Mesh = &CreateDefaultSubobject<SkeletalMeshComponent>();
    m_CharacterMovement = &CreateDefaultSubobject<CharacterMovementComponent>();

    // Character locomotion is movement-driven, so capsule body should follow component motion.
    m_Capsule->BodyType = ERBBodyType::Kinematic;
    m_Capsule->ObjectChannel = CollisionChannel::Pawn;

    SetRootComponent(m_Capsule);

    if (m_CameraAnchor)
    {
        m_CameraAnchor->AttachTo(m_Capsule, false);
        m_CameraAnchor->SetPosition(Vector3(0.0f, 0.0f, 1.6f));
    }

    if (m_Mesh)
        m_Mesh->AttachTo(m_Capsule, false);

    if (m_CharacterMovement)
        m_CharacterMovement->SetUpdatedComponent(m_Capsule);
}

void Character::BeginPlay()
{
    Pawn::BeginPlay();

    if (!m_Capsule)
        m_Capsule = GetObjectComponent<CapsuleComponent>();

    if (!m_Mesh)
        m_Mesh = GetObjectComponent<SkeletalMeshComponent>();

    if (!m_CameraAnchor)
        m_CameraAnchor = GetObjectComponent<CameraAnchorComponent>();

    if (!m_CharacterMovement)
        m_CharacterMovement = GetObjectComponent<CharacterMovementComponent>();

    if (m_CharacterMovement)
        m_CharacterMovement->SetUpdatedComponent(m_Capsule);

    // Character movement setup smoke tests (runtime invariants).
    CHECK_MSG(m_Capsule != nullptr, "Character BeginPlay failed: CapsuleComponent is missing.");
    CHECK_MSG(m_CameraAnchor != nullptr, "Character BeginPlay failed: CameraAnchorComponent is missing.");
    CHECK_MSG(m_Mesh != nullptr, "Character BeginPlay failed: SkeletalMeshComponent is missing.");
    CHECK_MSG(m_CharacterMovement != nullptr, "Character BeginPlay failed: CharacterMovementComponent is missing.");
    CHECK_MSG(GetRootComponent() == m_Capsule, "Character BeginPlay failed: Capsule must be the root component.");
    CHECK_MSG(
        !m_CharacterMovement || m_CharacterMovement->GetUpdatedComponent() == m_Capsule,
        "Character BeginPlay failed: CharacterMovementComponent must update the capsule.");
}

void Character::Tick(float dt)
{
    Pawn::Tick(dt);
    
}

void Character::LaunchCharacter(const Vector3& launchVelocity, const bool bXYOverride, const bool bZOverride)
{
    if (!m_CharacterMovement)
        m_CharacterMovement = GetObjectComponent<CharacterMovementComponent>();

    if (!m_CharacterMovement)
        return;

    m_CharacterMovement->LaunchCharacter(launchVelocity, bXYOverride, bZOverride);
}

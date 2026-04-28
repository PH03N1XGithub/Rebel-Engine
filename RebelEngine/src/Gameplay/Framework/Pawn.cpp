#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Gameplay/Framework/MovementComponent.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Gameplay/Framework/PlayerController.h"
#include "Engine/Scene/Scene.h"

Pawn::~Pawn()
{
}

void Pawn::BeginPlay()
{
	Actor::BeginPlay();

	m_PendingMovementInput = Vector3(0.0f);
	m_PendingLookInput = Vector2(0.0f);
	m_bJumpRequested = false;

	if (!GetRootComponent() && GetScene())
	{
		AddObjectComponent<SceneComponent>();
	}

	EnsureMovementComponent();
}

void Pawn::EndPlay()
{
	m_PendingMovementInput = Vector3(0.0f);
	m_PendingLookInput = Vector2(0.0f);
	m_bJumpRequested = false;
	m_MovementComponent = nullptr;
	Actor::EndPlay();
}

void Pawn::Tick(float dt)
{
	EnsureMovementComponent();

	Actor::Tick(dt);
}

void Pawn::AddMovementInput(const Vector3& worldDirection, const float scale)
{
	if (scale == 0.0f)
		return;

	const float lengthSq = glm::dot(worldDirection, worldDirection);
	if (lengthSq <= 0.000001f)
		return;

	const Vector3 normalizedDir = glm::normalize(worldDirection);
	m_PendingMovementInput += normalizedDir * scale;
}

void Pawn::AddLookInput(const Vector2& lookDelta)
{
	if (PlayerController* PC = static_cast<PlayerController*>(GetController()))
	{
		//PC->AddLookInput(lookDelta);
	}
	m_PendingLookInput += lookDelta;
}

void Pawn::Jump()
{
	m_bJumpRequested = true;
}

Vector3 Pawn::ConsumeMovementInput()
{
	const Vector3 pending = m_PendingMovementInput;
	m_PendingMovementInput = Vector3(0.0f);
	return pending;
}

bool Pawn::ConsumeJumpRequested()
{
	const bool bRequested = m_bJumpRequested;
	m_bJumpRequested = false;
	return bRequested;
}

void Pawn::SetController(Controller* controller)
{
	m_Controller = controller;
}

void Pawn::EnsureMovementComponent()
{
	if (m_MovementComponent)
		return;

	// Prefer CharacterMovementComponent when present so character input is consumed by character movement logic.
	if (CharacterMovementComponent* characterMovement = GetObjectComponent<CharacterMovementComponent>())
	{
		m_MovementComponent = characterMovement;
		return;
	}

	m_MovementComponent = GetObjectComponent<MovementComponent>();
	if (!m_MovementComponent && GetScene())
	{
		m_MovementComponent = &AddObjectComponent<MovementComponent>();
	}
}

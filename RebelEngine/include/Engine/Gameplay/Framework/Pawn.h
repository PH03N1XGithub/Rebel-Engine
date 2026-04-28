#pragma once
#include "Engine/Scene/Actor.h"

class Controller;
class MovementComponent;

class Pawn : public Actor
{
	REFLECTABLE_CLASS(Pawn,Actor)
public:
	Pawn() = default;
	~Pawn() override;
	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float dt) override;

	void AddMovementInput(const Vector3& worldDirection, float scale = 1.0f);
	void AddLookInput(const Vector2& lookDelta);
	void Jump();

	Vector3 ConsumeMovementInput();
	bool ConsumeJumpRequested();

	const Vector3& GetPendingMovementInput() const { return m_PendingMovementInput; }
	const Vector2& GetPendingLookInput() const { return m_PendingLookInput; }
	bool IsJumpRequested() const { return m_bJumpRequested; }

	void SetController(Controller* controller);
	Controller* GetController() const { return m_Controller; }
	MovementComponent* GetMovementComponent() const { return m_MovementComponent; }
private:
	void EnsureMovementComponent();

	Controller* m_Controller = nullptr;
	MovementComponent* m_MovementComponent = nullptr;
	Vector3 m_PendingMovementInput = Vector3(0.0f);
	Vector2 m_PendingLookInput = Vector2(0.0f);
	bool m_bJumpRequested = false;

};
REFLECT_CLASS(Pawn,Actor)
END_REFLECT_CLASS(Pawn)

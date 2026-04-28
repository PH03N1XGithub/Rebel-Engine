#pragma once
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Scene/Actor.h"

class Pawn;

class Controller : public Actor
{
	REFLECTABLE_CLASS(Controller,Actor)
public:
	Controller();
	~Controller() override;
	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float dt) override;
	
	virtual void Possess(Pawn* pawn);
	virtual void UnPossess();
	Pawn* GetPawn() const { return m_Pawn; }
	const Vector3& GetControlRotation() const { return m_ControlRotation; }
	void SetControlRotation(const Vector3& controlRotation) { m_ControlRotation = controlRotation; }

private:
	Pawn* m_Pawn = nullptr;
	Vector3 m_ControlRotation = Vector3(0.0f);
	
};
REFLECT_CLASS(Controller,Actor)
REFLECT_PROPERTY(Controller, m_ControlRotation,
	Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(Controller)


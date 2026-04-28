#include "Engine/Framework/EnginePch.h"
#include "Engine/Physics/PhysicsModule.h"

#include "Engine/Physics/PhysicsSystem.h"

PhysicsModule::~PhysicsModule()
{
}

PhysicsModule::PhysicsModule()
{
	m_TickType = TickType::Physics;
}

void PhysicsModule::Init()
{
	if (m_Physics)
		return;

	m_Physics = new PhysicsSystem();
	m_Physics->Init();
}

void PhysicsModule::Shutdown()
{
	if (!m_Physics)
		return;

	m_Physics->Shutdown();
	delete m_Physics;
	m_Physics = nullptr;
}

void PhysicsModule::Tick(float dt)
{
	(void)dt;

	if (!m_Physics)
		Init();
}


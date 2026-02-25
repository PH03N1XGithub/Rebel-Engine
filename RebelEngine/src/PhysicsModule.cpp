#include "EnginePch.h"
#include "PhysicsModule.h"

#include "BaseEngine.h"
#include "PhysicsSystem.h"
#include "Scene.h"

PhysicsModule::~PhysicsModule()
{
}

PhysicsModule::PhysicsModule()
{
	E_TickType = TickType::Physics;
}

void PhysicsModule::Init()
{
	m_Physics = new PhysicsSystem();
	m_Physics->Init();
}

void PhysicsModule::Shutdown()
{
	m_Physics->Shutdown();
	delete m_Physics;
}

void PhysicsModule::Tick(float dt)
{
	if (!GEngine)
		return;

	Scene* scene = GEngine->GetActiveScene();
	if (!scene)
		return;
	if (!GEngine->IsPlaying())
	{
		m_Physics->EditorDebugDraw(*scene);
		return;
	}

	if (!m_Physics)
		Init();

	m_Physics->Step(*scene, dt);
}

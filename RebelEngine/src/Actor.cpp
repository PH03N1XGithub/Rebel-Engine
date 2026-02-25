#include "EnginePch.h"
#include "Actor.h"
#include "Scene.h"

DEFINE_LOG_CATEGORY(actorLog)

void Actor::BeginPlay()
{
	RB_LOG(actorLog,debug,"Begin Play")
}

void Actor::Tick(float dt)
{
	//RB_LOG(actorLog,debug,"Tick {} ", (void*)this)
}

void Actor::SetCanEverTick(bool bCanTick)
{
	m_bCanEverTick = bCanTick;

	// If we turn this OFF, also disable tick.
	if (!m_bCanEverTick && m_bTickEnabled)
	{
		SetTickEnabled(false);
	}
}

void Actor::SetTickEnabled(bool bEnabled)
{
	// If we can't ever tick, ignore
	if (!m_bCanEverTick)
	{
		m_bTickEnabled = false;
		return;
	}

	if (m_bTickEnabled == bEnabled)
		return; // no change

	m_bTickEnabled = bEnabled;

	if (!IsValid())
		return;

	// Notify scene to add/remove from its tick list
	if (Scene* scene = m_Scene)
	{
		if (m_bTickEnabled)
		{
			scene->RegisterTickActor(this);
		}
		else
		{
			scene->UnregisterTickActor(this);
		}
	}
}

String Actor::GetName() const
{
	return m_Scene->GetRegistry().get<NameComponent>(m_Entity).Name;
}

void Actor::SetName(const String& name) const
{
	m_Scene->GetRegistry().get<NameComponent>(m_Entity).Name = name;
}


void testActor::Tick(float dt)
{
	//Actor::Tick(dt);
	if (m_test)
	{
		TraceHit OutHit;
		LineTraceSingle(GetComponent<SceneComponent>().Position,{},OutHit,CollisionChannel::Any,EDrawDebugTrace::ForOneFrame);
	}
}

void Actor::Destroy()
{
	//RemoveComponent<>()
	m_Scene->DestroyActor(this);
}

void Actor::DestroyAllComponents()
{
	if (!m_Scene)
		return;

	auto& reg = m_Scene->GetRegistry();

	// Detach hierarchy first (SceneComponent semantics)
	if (m_RootComponent)
	{
		m_RootComponent = nullptr;
	}

	// Destroy ECS entities for all object components
	for (auto& comp : m_Components)
	{
		if (!comp)
			continue;

		if (comp->ECSHandle != entt::null)
		{
			reg.destroy(comp->ECSHandle);   // removes T* from registry
			comp->ECSHandle = entt::null;
		}
	}

	// 3️⃣ Destroy C++ objects
	m_Components.Clear();
}


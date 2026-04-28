#include "Engine/Framework/EnginePch.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/World.h"
#include <glm/gtx/matrix_decompose.hpp>

DEFINE_LOG_CATEGORY(actorLog)

void Actor::BeginPlay()
{
	RB_LOG(actorLog, debug, "Begin Play")
}

void Actor::InternalBeginPlayIfNeeded()
{
	if (m_bHasBegunPlay)
		return;

	m_bHasBegunPlay = true;
	m_bHasEndedPlay = false;
	BeginPlay();
	BeginPlayComponentsIfNeeded();
}

void Actor::InternalEndPlayIfNeeded()
{
	if (!m_bHasBegunPlay || m_bHasEndedPlay)
		return;

	m_bHasEndedPlay = true;
	EndPlay();
}

void Actor::BeginPlayComponentsIfNeeded()
{
	for (auto& comp : m_Components)
	{
		if (!comp)
			continue;

		ActorComponent* actorComponent = dynamic_cast<ActorComponent*>(comp.Get());
		if (!actorComponent || actorComponent->HasBegunPlay())
			continue;

		actorComponent->BeginPlay();
		actorComponent->SetHasBegunPlay(true);
	}
}

void Actor::Tick(float dt)
{
	// RB_LOG(actorLog, debug, "Tick {} ", (void*)this)
}

void Actor::EndPlay()
{
}

void Actor::TickComponents(float dt)
{
	if (!m_bHasBegunPlay)
		return;

	for (auto& comp : m_Components)
	{
		if (!comp)
			continue;

		ActorComponent* actorComponent = dynamic_cast<ActorComponent*>(comp.Get());
		if (!actorComponent)
			continue;

		if (!actorComponent->HasBegunPlay())
		{
			actorComponent->BeginPlay();
			actorComponent->SetHasBegunPlay(true);
		}

		actorComponent->Tick(dt);
	}
}

void Actor::SetCanEverTick(bool bCanTick)
{
	m_bCanEverTick = bCanTick;

	// If we turn this OFF, also disable tick.
	if (!m_bCanEverTick && m_bTickEnabled)
	{
		SetActorTickEnabled(false);
	}
}

void Actor::SetActorTickEnabled(bool bEnabled)
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

void Actor::SetTickGroup(ActorTickGroup group)
{
	m_TickGroup = group;
}

void Actor::SetTickPriority(int priority)
{
	m_TickPriority = priority;
}

Mat4 Actor::GetActorTransform() const
{
	if (!m_RootComponent)
		return Mat4(1.0f);

	return m_RootComponent->GetWorldTransform();
}

void Actor::SetActorTransform(const Mat4& newTransform)
{
	if (!m_RootComponent)
		return;

	Vector3 scale(1.0f);
	Quaternion rotation(1.0f, 0.0f, 0.0f, 0.0f);
	Vector3 translation(0.0f);
	Vector3 skew(0.0f);
	Vector4 perspective(0.0f);

	if (!glm::decompose(newTransform, scale, rotation, translation, skew, perspective))
		return;

	m_RootComponent->SetPosition(translation);
	m_RootComponent->SetRotationQuat(glm::normalize(rotation));
	m_RootComponent->SetScale(scale);
}

Vector3 Actor::GetActorLocation() const
{
	if (!m_RootComponent)
		return Vector3(0.0f);

	return Vector3(GetActorTransform()[3]);
}

void Actor::SetActorLocation(const Vector3& newLocation)
{
	if (!m_RootComponent)
		return;

	m_RootComponent->SetPosition(newLocation);
}

Vector3 Actor::GetActorRotationEuler() const
{
	if (!m_RootComponent)
		return Vector3(0.0f);

	return m_RootComponent->GetRotationEuler();
}

void Actor::SetActorRotation(const Vector3& newRotation)
{
	if (!m_RootComponent)
		return;

	m_RootComponent->SetRotationEuler(newRotation);
}

Vector3 Actor::GetActorForwardVector() const
{
	const Mat4 world = GetActorTransform();
	const Vector3 forward = Vector3(world[0]);
	if (glm::length(forward) <= 0.0f)
		return Vector3(1.0f, 0.0f, 0.0f);

	return glm::normalize(forward);
}

Vector3 Actor::GetActorRightVector() const
{
	const Mat4 world = GetActorTransform();
	const Vector3 right = -Vector3(world[1]);
	if (glm::length(right) <= 0.0f)
		return Vector3(0.0f, -1.0f, 0.0f);

	return glm::normalize(right);
}

Vector3 Actor::GetActorUpVector() const
{
	const Mat4 world = GetActorTransform();
	const Vector3 up = Vector3(world[2]);
	if (glm::length(up) <= 0.0f)
		return Vector3(0.0f, 0.0f, 1.0f);

	return glm::normalize(up);
}

void Actor::AddActorWorldOffset(const Vector3& deltaLocation)
{
	SetActorLocation(GetActorLocation() + deltaLocation);
}

void Actor::AddActorWorldRotation(const Vector3& deltaRotationEuler)
{
	SetActorRotation(GetActorRotationEuler() + deltaRotationEuler);
}

void Actor::SetRootComponent(SceneComponent* newRoot)
{
	if (newRoot && newRoot->GetOwner() != this)
	{
		CHECK_MSG(false, "SetRootComponent: component owner mismatch");
		return;
	}

	m_RootComponent = newRoot;

	if (m_RootComponent)
	{
		m_RootComponent->m_Parent = nullptr;
		m_RootComponent->m_SceneRegistry = (m_Scene ? &m_Scene->GetRegistry() : nullptr);
	}
}

const String& Actor::GetName() const
{
	static const String kEmptyName = "";
	if (!m_Scene || m_Entity == entt::null)
		return kEmptyName;

	auto& registry = m_Scene->GetRegistry();
	if (!registry.valid(m_Entity) || !registry.all_of<NameComponent>(m_Entity))
		return kEmptyName;

	return registry.get<NameComponent>(m_Entity).Name;
}

void Actor::SetName(const String& newName)
{
	if (!m_Scene || m_Entity == entt::null)
		return;

	auto& registry = m_Scene->GetRegistry();
	if (!registry.valid(m_Entity) || !registry.all_of<NameComponent>(m_Entity))
		return;

	registry.get<NameComponent>(m_Entity).Name = newName;
}

void testActor::Tick(float dt)
{
	// Actor::Tick(dt);
	if (m_test)
	{
		TraceQueryParams params{};
		params.Channel = CollisionChannel::Any;

		TraceHit OutHit;
		LineTraceSingle(GetActorLocation(), GetActorLocation() + GetActorForwardVector(), OutHit, params,EDrawDebugTrace::ForOneFrame);
		LineTraceSingle(GetActorLocation(), GetActorLocation() + GetActorRightVector() * 2.f, OutHit, params);
		LineTraceSingle(GetActorLocation(), GetActorLocation() + GetActorUpVector() * 3.f, OutHit, params);
	}
}

void Actor::Destroy()
{
	InternalEndPlayIfNeeded();

	if (m_Scene)
	{
		m_Scene->DestroyActor(this);
	}
}

World* Actor::GetWorld() const
{
	return m_Scene ? m_Scene->GetWorld() : nullptr;
}

entt::registry* Actor::TryGetSceneRegistry()
{
	return m_Scene ? &m_Scene->GetRegistry() : nullptr;
}

const entt::registry* Actor::TryGetSceneRegistry() const
{
    return m_Scene ? &m_Scene->GetRegistry() : nullptr;
}

void Actor::RegisterPrimitivePointerIfNeeded(EntityComponent* component)
{
    entt::registry* registry = TryGetSceneRegistry();
    if (!registry || !component || component->GetECSHandle() == entt::null)
        return;

    PrimitiveComponent* primitive = dynamic_cast<PrimitiveComponent*>(component);
    if (!primitive)
        return;

    if (!registry->all_of<PrimitiveComponent*>(component->GetECSHandle()))
        registry->emplace<PrimitiveComponent*>(component->GetECSHandle(), primitive);
}

void Actor::HandleSceneComponentAdded(EntityComponent* component)
{
    SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component);
    if (!sceneComponent)
        return;

    if (!m_RootComponent)
    {
        SetRootComponent(sceneComponent);
        return;
    }

    if (sceneComponent != m_RootComponent)
    {
        sceneComponent->m_Parent = m_RootComponent;
        sceneComponent->m_SceneRegistry = TryGetSceneRegistry();
    }
}

void Actor::NotifyComponentCreated(EntityComponent* component)
{
    ActorComponent* actorComponent = dynamic_cast<ActorComponent*>(component);
    if (!actorComponent)
        return;

    actorComponent->OnCreate();
    if (m_bHasBegunPlay && !actorComponent->HasBegunPlay())
    {
        actorComponent->BeginPlay();
        actorComponent->SetHasBegunPlay(true);
    }
}

void Actor::RegisterPendingComponents()
{
    if (!m_Scene)
        return;

    auto& registry = m_Scene->GetRegistry();
    for (PendingComponentRegistration& pending : m_PendingComponentRegistrations)
    {
        if (!pending.Component || !pending.RegisterFn)
            continue;

        pending.Component->SetOwner(this);
        if (pending.Component->GetECSHandle() == entt::null)
            pending.RegisterFn(registry, pending.Component);

        RegisterPrimitivePointerIfNeeded(pending.Component);
        HandleSceneComponentAdded(pending.Component);
    }

    m_PendingComponentRegistrations.Clear();
}
void Actor::DestroyPhysicsBodyForComponent(EntityComponent* component)
{
	if (!component)
		return;

	PrimitiveComponent* primitive = dynamic_cast<PrimitiveComponent*>(component);
	if (!primitive || !primitive->IsBodyCreated())
		return;

	World* world = GetWorld();
	if (!world)
	{
		primitive->ClearBodyHandle();
		return;
	}

	PhysicsSystem* physics = world->TryGetPhysics();
	if (!physics)
	{
		primitive->ClearBodyHandle();
		return;
	}

	physics->DestroyBodyForComponent(*primitive);
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

		DestroyPhysicsBodyForComponent(comp.Get());
		comp->SetHasBegunPlay(false);

		if (comp->GetECSHandle() != entt::null)
		{
			reg.destroy(comp->GetECSHandle()); // removes T* from registry
			comp->SetECSHandle(entt::null);
		}
	}

	// Destroy C++ objects
	m_Components.Clear();
	m_PendingComponentRegistrations.Clear();
	m_bHasBegunPlay = false;
	m_bHasEndedPlay = false;
}

bool Actor::RemoveObjectComponentInstance(EntityComponent* component)
{
	if (!component || component->GetOwner() != this || !m_Scene)
		return false;

	auto& reg = m_Scene->GetRegistry();
	SceneComponent* removedSceneComponent = dynamic_cast<SceneComponent*>(component);

	if (removedSceneComponent && removedSceneComponent == m_RootComponent)
		return false;

	if (removedSceneComponent)
	{
		for (auto& ownedComponent : m_Components)
		{
			SceneComponent* sceneComponent = ownedComponent ? dynamic_cast<SceneComponent*>(ownedComponent.Get()) : nullptr;
			if (sceneComponent && sceneComponent->GetParent() == removedSceneComponent)
				sceneComponent->AttachTo(m_RootComponent, true);
		}
	}

	for (uint32 i = 0; i < m_Components.Num(); ++i)
	{
		if (m_Components[i].Get() != component)
			continue;

		DestroyPhysicsBodyForComponent(component);
		component->SetHasBegunPlay(false);

		if (component->GetECSHandle() != entt::null)
		{
			reg.destroy(component->GetECSHandle());
			component->SetECSHandle(entt::null);
		}

		m_Components.EraseAtSwap(i);
		return true;
	}

	return false;
}

String Actor::MakeUniqueComponentEditorName(const String& baseName, const EntityComponent* ignore) const
{
	const String cleanBase = baseName.length() > 0 ? baseName : String("Component");
	auto nameExists = [this, ignore](const String& candidate)
	{
		for (const auto& componentPtr : m_Components)
		{
			const EntityComponent* component = componentPtr.Get();
			if (!component || component == ignore)
				continue;

			if (component->GetEditorName() == candidate)
				return true;
		}

		return false;
	};

	if (!nameExists(cleanBase))
		return cleanBase;

	for (int index = 1; index < 10000; ++index)
	{
		const String candidate = cleanBase + " " + String(std::to_string(index).c_str());
		if (!nameExists(candidate))
			return candidate;
	}

	return cleanBase + " 9999";
}







// ActorTemplates.h
#pragma once

#include <typeindex>

template<typename T, typename... Args>
T& Actor::AddObjectComponent(Args&&... args)
{
	static_assert(std::is_base_of_v<EntityComponent, T>);

	auto comp = RMakeUnique<T>(std::forward<Args>(args)...);
	comp->SetOwner(this);

	T* raw = comp.Get();
	const String componentTypeName = T::StaticType() ? T::StaticType()->Name : String("Component");
	raw->SetEditorName(MakeUniqueComponentEditorName(componentTypeName));

	if (entt::registry* registry = TryGetSceneRegistry())
	{
		comp->SetECSHandle(registry->create());
		registry->emplace<T*>(comp->GetECSHandle(), comp.Get());
		RegisterPrimitivePointerIfNeeded(raw);
	}
	else
	{
		comp->SetECSHandle(entt::null);
		PendingComponentRegistration pending{};
		pending.Component = raw;
		pending.RegisterFn = [](entt::registry& registry, EntityComponent* component)
		{
			if (!component)
				return;

			auto* typedComponent = dynamic_cast<T*>(component);
			if (!typedComponent)
				return;

			typedComponent->SetECSHandle(registry.create());
			registry.emplace<T*>(typedComponent->GetECSHandle(), typedComponent);
		};

		m_PendingComponentRegistrations.Emplace(pending);
	}

	m_Components.Add(std::move(comp));
	HandleSceneComponentAdded(raw);
	NotifyComponentCreated(raw);

	return *raw;
}

template<typename T, typename... Args>
T& Actor::CreateDefaultSubobject(Args&&... args)
{
	return AddObjectComponent<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T& Actor::AddDataComponent(Args&&... args)
{
	static_assert(!std::is_base_of_v<EntityComponent, T>,
				  "Data components must not derive from EntityComponent");

	CHECK_MSG(m_Entity != entt::null, "Actor has no ECS entity!");
	entt::registry* registry = TryGetSceneRegistry();
	CHECK_MSG(registry, "Actor has no Scene registry!");

	return registry->emplace<T>(m_Entity, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T& Actor::AddComponent(Args&&... args)
{
	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		return AddObjectComponent<T>(std::forward<Args>(args)...);
	}
	else
	{
		return AddDataComponent<T>(std::forward<Args>(args)...);
	}
}

template<typename T>
T* Actor::GetObjectComponent()
{
	static_assert(std::is_base_of_v<EntityComponent, T>,
				  "GetObjectComponent<T> requires EntityComponent type");

	for (auto& comp : m_Components)
	{
		if (auto ptr = dynamic_cast<T*>(comp.Get()))
			return ptr;
	}

	return nullptr;
}

template<typename T>
const T* Actor::GetObjectComponent() const
{
	static_assert(std::is_base_of_v<EntityComponent, T>,
				  "GetObjectComponent<T> requires EntityComponent type");

	for (const auto& comp : m_Components)
	{
		if (auto ptr = dynamic_cast<T*>(comp.Get()))
		{
			if (T::StaticType() == ptr->GetType())
			{
				return ptr;
			}
		}
	}

	return nullptr;
}

template<typename T>
T& Actor::GetComponent()
{
	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		T* comp = GetObjectComponent<T>();
		CHECK_MSG(comp, "Actor does not have object component");
		return *comp;
	}
	else
	{
		entt::registry* registry = TryGetSceneRegistry();
		CHECK_MSG(registry, "Actor has no Scene registry!");
		return registry->get<T>(m_Entity);
	}
}

template<typename T>
const T& Actor::GetComponent() const
{
	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		const T* comp = GetObjectComponent<T>();
		CHECK_MSG(comp, "Actor does not have object component");
		return *comp;
	}
	else
	{
		const entt::registry* registry = TryGetSceneRegistry();
		CHECK_MSG(registry, "Actor has no Scene registry!");
		return registry->get<T>(m_Entity);
	}
}

template<typename T>
bool Actor::HasComponent() const
{
	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		return GetObjectComponent<T>() != nullptr;
	}
	else
	{
		const entt::registry* registry = TryGetSceneRegistry();
		CHECK_MSG(registry, "Actor has no Scene registry!");
		return registry->all_of<T>(m_Entity);
	}
}

template<class T>
void Actor::RemoveComponent()
{
	entt::registry* reg = TryGetSceneRegistry();
	CHECK_MSG(reg, "Actor has no Scene registry!");

	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		for (uint32 i = 0; i < m_Components.Num(); ++i)
		{
			EntityComponent* base = m_Components[i].Get();
			T* comp = dynamic_cast<T*>(base);
			if (!comp)
				continue;

			DestroyPhysicsBodyForComponent(comp);
			comp->SetHasBegunPlay(false);

			if (comp->GetECSHandle() != entt::null)
			{
				reg->destroy(comp->GetECSHandle());
				comp->SetECSHandle(entt::null);
			}

			m_Components.EraseAtSwap(i);
			return;
		}

		CHECK_MSG(false, "RemoveComponent: Actor does not have object component");
	}
	else
	{
		CHECK_MSG(m_Entity != entt::null, "Actor has no ECS entity!");
		reg->remove<T>(m_Entity);
	}
}

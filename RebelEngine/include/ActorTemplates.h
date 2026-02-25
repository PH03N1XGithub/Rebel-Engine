// ActorTemplates.h
#pragma once
#include <typeindex>


template<typename T, typename... Args>
T& Actor::AddObjectComponent(Args&&... args)
{
	static_assert(std::is_base_of_v<EntityComponent, T>);

	CHECK_MSG(m_Scene, "Actor has no Scene!");

	auto comp = RMakeUnique<T>(std::forward<Args>(args)...);
	comp->Owner = this;

	auto& registry = m_Scene->GetRegistry();
	comp->ECSHandle = registry.create();
	if constexpr (std::is_base_of_v<PrimitiveComponent, T>)
	{
		registry.emplace<PrimitiveComponent*>(comp->ECSHandle, comp.Get());
	}
	else
	{
		registry.emplace<T*>(comp->ECSHandle, comp.Get());
	}

	T* raw = comp.Get();

	// Root assignment (Unreal semantics)
	if constexpr (std::is_base_of_v<SceneComponent, T>)
	{
		if (m_RootComponent == nullptr)
		{
			m_RootComponent = static_cast<SceneComponent*>(raw);
		}
		else
		{
			raw->Parent = m_RootComponent->ECSHandle;
			raw->SceneRegistry = &m_Scene->GetRegistry();
		}
	}

	m_Components.Add(std::move(comp));
	return *raw;
}

template<typename T, typename... Args>
T& Actor::AddDataComponent(Args&&... args)
{
	static_assert(!std::is_base_of_v<EntityComponent, T>,
				  "Data components must not derive from EntityComponent");

	CHECK_MSG(m_Entity != entt::null, "Actor has no ECS entity!");

	auto& registry = m_Scene->GetRegistry();
	return registry.emplace<T>(m_Entity, std::forward<Args>(args)...);
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

	/*T& comp = m_Scene->GetRegistry().emplace<T>(
		m_Entity, std::forward<Args>(args)...
	);
	if constexpr (std::is_base_of_v<SceneComponent, T> &&  !std::is_same_v<SceneComponent, T>)
	{
		comp.Parent   = m_Entity;   // engine-level handle
		comp.SceneRegistry = &m_Scene->GetRegistry();
	}
	
	return comp;*/
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

	for (auto& comp : m_Components)
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
		return m_Scene->GetRegistry().get<T>(m_Entity);
	}
}

template<typename T>
const T& Actor::GetComponent() const
{
	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		const T* comp = GetObjectComponent<T>();
		CHECK_MSG(comp , "Actor does not have object component");
		return *comp;
	}
	else
	{
		return m_Scene->GetRegistry().get<T>(m_Entity);
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
		return m_Scene->GetRegistry().all_of<T>(m_Entity);
	}
}

template<class T>
void Actor::RemoveComponent()
{
	CHECK_MSG(m_Scene, "Actor has no Scene!");

	if constexpr (std::is_base_of_v<EntityComponent, T>)
	{
		// 1) find the object component
		for (uint32 i = 0; i < m_Components.Num(); ++i)
		{
			EntityComponent* base = m_Components[i].Get();
			T* comp = dynamic_cast<T*>(base);
			if (!comp) continue;

			auto& reg = m_Scene->GetRegistry();

			// 2) handle SceneComponent hierarchy/root semantics
			if constexpr (std::is_base_of_v<SceneComponent, T>)
			{
				if (comp == m_RootComponent)
				{
					// decide your rule: forbid removing root, or re-root
					// simplest: forbid
					CHECK_MSG(false, "Cannot remove Root SceneComponent yet (implement re-rooting).");
				}

				comp->DetachFromParent();
				comp->DetachAllChildren(); // or reparent children
			}

			// 3) destroy the ECS entity that indexes this component
			if (comp->ECSHandle != entt::null)
			{
				reg.destroy(comp->ECSHandle);
				comp->ECSHandle = entt::null;
			}

			// 4) delete the C++ object (UniquePtr)
			m_Components.EraseAtSwap(i);
			return;
		}

		CHECK_MSG(false, "RemoveComponent: Actor does not have object component");
	}
	else
	{
		CHECK_MSG(m_Entity != entt::null, "Actor has no ECS entity!");
		m_Scene->GetRegistry().remove<T>(m_Entity);
	}
}











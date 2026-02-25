// Scene.h
#pragma once

#include <ThirdParty/entt.h>
#include <type_traits>

#include "Actor.h"
#include "Components.h"
#include "Core/Serialization/YamlSerializer.h"

class Scene
{
public:
	Scene()  = default;
	~Scene() = default;

	// ---------- SPAWN ----------
	Actor& SpawnActor(const Rebel::Core::Reflection::TypeInfo* type);

	template<typename T = Actor>
	T& SpawnActor()
	{
		static_assert(std::is_base_of_v<Actor, T>);
		return static_cast<T&>(SpawnActor(T::StaticType()));
	}
	
	// Destroy by pointer to Actor
	void DestroyActor(Actor* actor);
	void FlushPendingActorDestroy();

	// If you still want "destroy by entity", keep this:
	void DestroyEntity(entt::entity e)
	{
		if (e != entt::null)
			m_Registry.destroy(e);
	}

	// -------- Tick registration API --------
	void RegisterTickActor(Actor* actor);
	void UnregisterTickActor(Actor* actor);

	entt::registry&       GetRegistry()       { return m_Registry; }
	const entt::registry& GetRegistry() const { return m_Registry; }

	TArray<RUniquePtr<Actor>,16>& GetActors() { return m_Actors; }

	void BeginPlay();
	void Tick(float dt);

	void UpdateTransforms();

	Actor* GetActor(entt::entity e)
	{
		Actor** found = m_ActorsMap.Find(e);
		return found ? *found : nullptr;
	}

	const Actor* GetActor(entt::entity e) const
	{
		const Actor* const* found = m_ActorsMap.Find(e);
		return found ? *found : nullptr;
	}
	Rebel::Core::Serialization::YamlSerializer m_Serializer;
	

	void Serialize(String name)
	{
		
		m_Serializer.BeginObject("Scene");

		m_Serializer.BeginArray("Actors");

		for (const auto& actorPtr : m_Actors)
		{
			Actor& actor = *actorPtr.Get();

			m_Serializer.BeginArrayElement();

			// Actor data
			m_Serializer.SerializeType(actor.GetType(),&actor);

			// Components
			m_Serializer.BeginObject("Components");
			SerializeActorComponents(m_Serializer, actor);
			m_Serializer.EndObject();

			m_Serializer.EndArrayElement();
		}


		m_Serializer.EndObject(); // Actors
		m_Serializer.EndObject(); // Scene

		if (m_Serializer.SaveToFile(name.c_str()))
			std::cout << name.c_str() << " saved successfully\n";
		else
			std::cout << "Failed to save scene.Ryml\n"; 
	}

	void SerializeActorComponents(
		Rebel::Core::Serialization::YamlSerializer& serializer,
		Actor& actor)
	{
		auto& registry = m_Registry;
		entt::entity e = actor.GetHandle();

		for (const auto& comp : ComponentRegistry::Get().GetComponents())
		{
			if (!comp.HasFn || !comp.GetFn)
				continue;

			if (!comp.HasFn(actor))
				continue;

			void* componentPtr = comp.GetFn(actor);

			// Reflection-only serialization
			serializer.SerializeType(comp.Type, componentPtr);

			
		}
	}


	bool Deserialize(const String& filename);
	void Clear(); // optional helper


	CameraComponent* FindPrimaryCamera()
	{
		auto view = m_Registry.view<CameraComponent*>();

		CameraComponent* fallback = nullptr;

		for (auto entity : view)
		{
			auto& cam = view.get<CameraComponent*>(entity);

			// First camera we see becomes fallback
			/*if (!fallback)
				fallback = &cam;*/

			// Explicit primary camera wins
			if (cam->bPrimary)
				return cam;
		}

		return fallback;
	}


private:

	void UpdateTransformRecursive(entt::entity entity, const Mat4& parentWorld);

	/*void UpdateComponentWorldTransforms()
	{
		auto view = m_Registry.view<SceneComponent>();

		for (auto entity : view)
		{
			SceneComponent& sc = view.get<SceneComponent>(entity);

			// Get owning entity / actor world transform
			Mat4 entityWorld = sc.GetWorldTransform();

			sc.UpdateWorldTransform(entityWorld);
		}
	}*/

	
	// ---------- ECS ----------
	entt::registry m_Registry;

	// ---------- Actor ownership ----------
	TArray<RUniquePtr<Actor>, 16> m_Actors;   // owns Actors (unique_ptr)
	TMap<entt::entity,Actor*> m_ActorsMap;   // owns Actors (unique_ptr)

	// ---------- Tick manager (tiny) ----------
	TArray<Actor*, 16> m_TickActors;          // who ticks this frame
	TArray<Actor*, 16> m_PendingTickAdd;      // spawned during Tick
	TArray<Actor*, 16> m_PendingTickRemove;   // destroyed during Tick
	TArray<Actor*, 16> m_PendingDestroyActors;
	Bool m_IsTickingActors = false;

	
};

// component template impls
#include "ActorTemplates.h"

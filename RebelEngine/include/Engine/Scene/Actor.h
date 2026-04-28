// Actor.h
#pragma once

#include <ThirdParty/entt.h>
//#include "Engine/Components/Components.h"


struct EntityComponent;
struct SceneComponent;
class Scene; // forward declaration
class World;

enum class ActorTickGroup
{
	PrePhysics,
	PostPhysics,
	PostUpdate
};

class Actor
{

	REFLECTABLE_CLASS(Actor,void)
	

public:
	Actor(){}
	virtual ~Actor() = default;

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	Actor(Actor&&) = default;
	Actor& operator=(Actor&&) = default;

	// -------- Tick API --------
	bool CanEverTick() const      { return m_bCanEverTick; }
	bool IsActorTickEnabled() const { return m_bTickEnabled; }
	bool IsTickEnabled() const    { return IsActorTickEnabled(); } // compatibility
	bool HasBegunPlay() const     { return m_bHasBegunPlay; }
	bool IsPendingDestroy() const { return m_bPendingDestroy; }
	ActorTickGroup GetTickGroup() const { return m_TickGroup; }
	int GetTickPriority() const { return m_TickPriority; }

	// Call in derived constructors (like UE)
	void SetCanEverTick(bool bCanTick);

	// Call any time (also at runtime if you want)
	void SetActorTickEnabled(bool bEnabled);
	void SetTickEnabled(bool bEnabled) { SetActorTickEnabled(bEnabled); } // compatibility
	void SetTickGroup(ActorTickGroup group);
	void SetTickPriority(int priority);
	
	// -------- Component API (declarations only) --------

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);
	template <class T>
	T* GetObjectComponent();
	template<typename T>
   const T* GetObjectComponent() const;
	template <typename T, typename... Args>
	T& AddObjectComponent(Args&&... args);
	template <typename T, typename... Args>
	T& CreateDefaultSubobject(Args&&... args);
	template <class T, class ... Args>
	T& AddDataComponent(Args&&... args);

	template<typename T>
	T& GetComponent();

	template<typename T>
	const T& GetComponent() const;

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	void RemoveComponent();
	
	void DestroyAllComponents();
	const TArray<RUniquePtr<EntityComponent>>& GetObjectComponents() const { return m_Components; }
	bool RemoveObjectComponentInstance(EntityComponent* component);
	String MakeUniqueComponentEditorName(const String& baseName, const EntityComponent* ignore = nullptr) const;

	void Destroy();

	// -------- Transform helpers (use SceneComponent / MeshComponent internally) --------

	Vector3 GetActorLocation() const;
	void SetActorLocation(const Vector3& newLocation);
	Vector3 GetActorRotationEuler() const;
	void SetActorRotation(const Vector3& newRotation);
	Mat4 GetActorTransform() const;
	void SetActorTransform(const Mat4& newTransform);

	Vector3 GetActorForwardVector() const;
	Vector3 GetActorRightVector() const;
	Vector3 GetActorUpVector() const;
	void AddActorWorldOffset(const Vector3& deltaLocation);
	void AddActorWorldRotation(const Vector3& deltaRotationEuler);

	// Optional: name helpers using TagComponent
	const String& GetName() const;
	void SetName(const String& newName);

	// --- Hierarchy API ---
	SceneComponent* GetRootComponent() const { return m_RootComponent; }
	void SetRootComponent(SceneComponent* newRoot);

	// -------- Misc --------

	bool IsValid() const { return m_Entity != entt::null && m_Scene != nullptr; }

	entt::entity GetHandle() const { return m_Entity; }
	Scene* GetScene()  const { return m_Scene; }
	World* GetWorld() const;

	explicit operator bool() const { return IsValid(); }

	bool operator==(const Actor& other) const
	{
		return m_Entity == other.m_Entity && m_Scene == other.m_Scene;
	}
	bool operator==(const entt::entity& other) const
	{
		return m_Entity == other;
	}

	bool operator!=(const Actor& other) const
	{
		return !(*this == other);
	}

	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void Tick(float dt);


private:
	friend class Scene; // let Scene touch tick internals if needed

	struct PendingComponentRegistration
	{
		EntityComponent* Component = nullptr;
		void (*RegisterFn)(entt::registry&, EntityComponent*) = nullptr;
	};

	void Init(const entt::entity handle, Scene* scene)
	{
		m_Entity = handle;
		m_Scene = scene;
		RegisterPendingComponents();
	}

	void InternalBeginPlayIfNeeded();
	void InternalEndPlayIfNeeded();
	void BeginPlayComponentsIfNeeded();
	void TickComponents(float dt);
	void DestroyPhysicsBodyForComponent(EntityComponent* component);
	void RegisterPendingComponents();
	void RegisterPrimitivePointerIfNeeded(EntityComponent* component);
	void HandleSceneComponentAdded(EntityComponent* component);
	void NotifyComponentCreated(EntityComponent* component);
	entt::registry* TryGetSceneRegistry();
	const entt::registry* TryGetSceneRegistry() const;

private:
	entt::entity m_Entity{ entt::null };
	Scene*       m_Scene = nullptr;

	SceneComponent* m_RootComponent = nullptr;
	TArray<RUniquePtr<EntityComponent>> m_Components;
	TArray<PendingComponentRegistration, 16> m_PendingComponentRegistrations;

	// -------- Tick state --------
	bool		m_bCanEverTick = true; // like AActor::PrimaryActorTick.bCanEverTick
	bool		m_bTickEnabled = true; // like PrimaryActorTick.IsTickFunctionEnabled()
	bool		m_bInTickList = false; // internal: is this registered in Scene::m_TickActors?
	bool		m_bHasBegunPlay = false;
	bool		m_bHasEndedPlay = false;
	bool		m_bPendingDestroy = false;
	ActorTickGroup m_TickGroup = ActorTickGroup::PrePhysics;
	int m_TickPriority = 0;
};
REFLECT_CLASS(Actor, void)
REFLECT_PROPERTY(Actor, m_TickPriority, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(Actor)


class testActor : public Actor
{
	REFLECTABLE_CLASS(testActor, Actor)

public:
	void Tick(float dt) override;
	Bool m_test = true;
	Bool m_test2 = true;
};
REFLECT_CLASS(testActor, Actor)
REFLECT_PROPERTY(testActor, m_test, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(testActor, m_test2, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(testActor)




// component template impls
#include "Engine/Scene/ActorTemplates.h"






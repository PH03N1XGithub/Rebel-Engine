// Actor.h
#pragma once

#include <ThirdParty/entt.h>
//#include "Components.h"


struct EntityComponent;
struct SceneComponent;
class Scene; // forward declaration

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
	bool IsTickEnabled() const    { return m_bTickEnabled; }

	// Call in derived constructors (like UE)
	void SetCanEverTick(bool bCanTick);

	// Call any time (also at runtime if you want)
	void SetTickEnabled(bool bEnabled);
	
	// -------- Component API (declarations only) --------

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);
	template <class T>
	T* GetObjectComponent();
	template<typename T>
   const T* GetObjectComponent() const;
	template <typename T, typename... Args>
	T& AddObjectComponent(Args&&... args);
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

	void Destroy();

	// -------- Transform helpers (use SceneComponent / MeshComponent internally) --------

	Mat4 GetActorTransform() const;
	void SetActorTransform(const Mat4& m);

	Vector3 GetActorLocation() const;
	void SetActorLocation(const Vector3& pos);

	// Optional: name helpers using TagComponent
	String GetName() const;
	void SetName(const String& name) const;

	// --- Hierarchy API ---

	bool HasParent() const;
	Actor* GetParent() const;               // call only if HasParent() == true
	void SetParent(const Actor& newParent, bool keepWorldTransform = true);
	void DetachFromParent(bool keepWorldTransform = true);
	std::vector<Actor> GetChildren() const;

	// -------- Misc --------

	bool IsValid() const { return m_Entity != entt::null && m_Scene != nullptr; }

	entt::entity GetHandle() const { return m_Entity; }
	Scene* GetScene()  const { return m_Scene; }

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

	 SceneComponent* GetRootComponent() { return m_RootComponent; }


private:
	friend class Scene; // let Scene touch tick internals if needed

	void Init(const entt::entity handle, Scene* scene)
	{
		m_Entity = handle;
		m_Scene = scene;
	}
public:
	
	virtual void BeginPlay();
	virtual void Tick(float dt);
	
	entt::entity m_Entity{ entt::null };
	Scene*       m_Scene = nullptr;

	SceneComponent* m_RootComponent = nullptr;
	TArray<RUniquePtr<EntityComponent>> m_Components;


	// -------- Tick state --------
	bool		m_bCanEverTick = true; // like AActor::PrimaryActorTick.bCanEverTick
	bool		m_bTickEnabled = true; // like PrimaryActorTick.IsTickFunctionEnabled()
	bool		m_bInTickList = false; // internal: is this registered in Scene::m_TickActors?
	bool		bPendingDestroy = false;
};
REFLECT_CLASS(Actor, void)
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

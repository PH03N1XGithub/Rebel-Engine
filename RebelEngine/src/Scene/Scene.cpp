// Scene.cpp
#include "Engine/Framework/EnginePch.h"
#include "Engine/Scene/Scene.h"
#include <algorithm>

#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Gameplay/Framework/GameMode.h"
#include "Engine/Scene/ActorTemplateSerializer.h"
#include "Engine/Scene/World.h"

// ---------- BeginPlay ----------

void Scene::Serialize(String name)
{
    m_Serializer.Reset();
    m_Serializer.BeginObject("Scene");

    if (m_World)
    {
        if (GameMode* gameMode = m_World->GetGameMode())
        {
            m_Serializer.BeginObject("GameMode");
            m_Serializer.Write("Type", gameMode->GetType() ? gameMode->GetType()->Name : String("GameMode"));
            m_Serializer.SerializeType(gameMode->GetType(), gameMode);
            m_Serializer.EndObject();
        }
    }

    m_Serializer.BeginArray("Actors");

    for (const auto& actorPtr : m_Actors)
    {
        Actor& actor = *actorPtr.Get();

        m_Serializer.BeginArrayElement();
        ActorTemplateSerializer::SerializeActorTemplate(m_Serializer, actor, { true });
        m_Serializer.EndArrayElement();
    }

    m_Serializer.EndObject();
    m_Serializer.EndObject();

    if (m_Serializer.SaveToFile(name.c_str()))
        std::cout << name.c_str() << " saved successfully\n";
    else
        std::cout << "Failed to save scene.Ryml\n";
}

void Scene::BeginPlay()
{
    if (m_bHasBegunPlay)
        return;

    m_bHasBegunPlay = true;

    // Allow actors to spawn additional actors during BeginPlay without invalidating iteration.
    for (uint32 index = 0; index < m_Actors.Num(); ++index)
    {
        Actor* actor = m_Actors[index].Get();
        if (!actor)
            continue;

        actor->InternalBeginPlayIfNeeded();
    }
}

// ---------- DestroyActor ----------
DEFINE_LOG_CATEGORY(actorLog)
Actor& Scene::SpawnActor(const TypeInfo* type)
{
    return SpawnActor(type, false);
}

Actor& Scene::SpawnActor(const TypeInfo* type, const bool bDeferredBeginPlay)
{
    CHECK_MSG(type, "SpawnActor: type is null!");
    CHECK_MSG(type->IsA(Actor::StaticType()), "SpawnActor: type is not an Actor!");
    CHECK_MSG(type->CreateInstance != nullptr, "SpawnActor: type is abstract (no factory)!");

    // 1) create entity
    entt::entity e = m_Registry.create();

    // âœ… CALL the factory
    Actor* actor = static_cast<Actor*>(type->CreateInstance());
    CHECK(actor);

    actor->Init(e, this);

    actor->AddComponent<IDComponent>();
    actor->AddComponent<ActorTagComponent>();
    if (!actor->GetRootComponent())
    {
        actor->AddComponent<SceneComponent>();
    }
    actor->AddComponent<NameComponent>();

    m_Actors.Emplace(RUniquePtr<Actor>(actor));
    m_ActorsMap.Add(e, actor);
    

    if (actor->CanEverTick() && actor->IsTickEnabled())
        RegisterTickActor(actor);

    // Runtime spawn contract: actors spawned after scene begin play are initialized immediately.
    if (m_bHasBegunPlay && !bDeferredBeginPlay)
        actor->InternalBeginPlayIfNeeded();
    
    
    RB_LOG(actorLog, info, "Spawn requested type={}, created dynamic type={}",
       type->Name, actor->GetType()->Name)


    return *actor;

}

void Scene::FinalizeDeferredActorSpawn(Actor& actor)
{
    if (m_bHasBegunPlay && actor.IsValid() && !actor.HasBegunPlay())
        actor.InternalBeginPlayIfNeeded();
}

void Scene::DestroyActor(Actor* actor)
{
    if (!actor || actor->IsPendingDestroy())
        return;

    actor->InternalEndPlayIfNeeded();
    actor->m_bPendingDestroy = true;

    // Remove from tick list safely
    UnregisterTickActor(actor);

    m_PendingDestroyActors.Emplace(actor);
}

void Scene::FlushPendingActorDestroy()
{
    for (Actor* actor : m_PendingDestroyActors)
    {
        // 1ï¸âƒ£ Destroy object components (ECS + C++)
        actor->DestroyAllComponents();

        entt::entity e = actor->GetHandle();

        // 2ï¸âƒ£ Remove lookup
        if (e != entt::null)
            m_ActorsMap.Remove(e);

        // 3ï¸âƒ£ Destroy actor ECS entity
        if (e != entt::null)
            m_Registry.destroy(e);

        // 4ï¸âƒ£ Delete actor object
        for (uint32 i = 0; i < m_Actors.Num(); ++i)
        {
            if (m_Actors[i].Get() == actor)
            {
                m_Actors.EraseAtSwap(i);
                break;
            }
        }
    }

    m_PendingDestroyActors.Clear();
}


// ---------- Tick ----------

void Scene::PrepareTick()
{
    m_IsTickingActors = true;

    m_PrePhysicsTickActors.Clear();
    m_PostPhysicsTickActors.Clear();
    m_PostUpdateTickActors.Clear();

    const uint64 count = m_TickActors.Num();
    for (uint64 i = 0; i < count; ++i)
    {
        Actor* actor = m_TickActors[i];
        if (!actor || actor->IsPendingDestroy())
            continue;

        switch (actor->GetTickGroup())
        {
        case ActorTickGroup::PrePhysics:
            m_PrePhysicsTickActors.Emplace(actor);
            break;
        case ActorTickGroup::PostPhysics:
            m_PostPhysicsTickActors.Emplace(actor);
            break;
        case ActorTickGroup::PostUpdate:
            m_PostUpdateTickActors.Emplace(actor);
            break;
        }
    }

    auto sortByPriority = [](TArray<Actor*, 16>& actors)
    {
        std::stable_sort(
            actors.begin(),
            actors.end(),
            [](const Actor* lhs, const Actor* rhs)
            {
                return lhs->GetTickPriority() < rhs->GetTickPriority();
            });
    };

    sortByPriority(m_PrePhysicsTickActors);
    sortByPriority(m_PostPhysicsTickActors);
    sortByPriority(m_PostUpdateTickActors);
}

void Scene::TickGroup(ActorTickGroup group, float dt)
{
    TArray<Actor*, 16>* bucket = nullptr;

    switch (group)
    {
    case ActorTickGroup::PrePhysics:
        bucket = &m_PrePhysicsTickActors;
        break;
    case ActorTickGroup::PostPhysics:
        bucket = &m_PostPhysicsTickActors;
        break;
    case ActorTickGroup::PostUpdate:
        bucket = &m_PostUpdateTickActors;
        break;
    }

    if (!bucket)
        return;

    for (Actor* actor : *bucket)
    {
        if (!actor || actor->IsPendingDestroy())
            continue;

        actor->Tick(dt);
        actor->TickComponents(dt);
    }
}

void Scene::FinalizeTick()
{
    m_IsTickingActors = false;

    UpdateTransforms();

    // Apply removals
    for (Actor* a : m_PendingTickRemove)
    {
        for (uint32 i = 0; i < m_TickActors.Num(); ++i)
        {
            if (m_TickActors[i] == a)
            {
                m_TickActors.RemoveAt(i);
                break;
            }
        }
        a->m_bInTickList = false;
    }
    m_PendingTickRemove.Clear();

    // Apply additions
    for (Actor* a : m_PendingTickAdd)
    {
        if (!a->m_bInTickList)
        {
            m_TickActors.Emplace(a);
            a->m_bInTickList = true;
        }
    }
    m_PendingTickAdd.Clear();

    FlushPendingActorDestroy();
}

void Scene::UpdateTransforms()
{
    auto& reg = m_Registry;
    auto view = reg.view<SceneComponent*>();

    // First: update all roots (no parent)
    for (auto entity : view)
    {
        // Root: parentWorld = identity
        UpdateTransform(entity);
        
    }
}

void Scene::UpdateTransform(entt::entity entity)
{
    auto& reg = m_Registry;
    auto& sc  = reg.get<SceneComponent*>(entity);

    // Build local from Position / RotationQuat / Scale
    Mat4 local = sc->GetLocalTransform();

    // World = ParentWorld * Local
    //sc->m_WorldTransform = local;
}


// -------- Tick registration helpers --------

void Scene::RegisterTickActor(Actor* actor)
{
    if (!actor || !actor->CanEverTick() || !actor->IsTickEnabled())
        return;

    // Already in list? (cheap linear scan, small N)
    if (actor->m_bInTickList)
        return;

    if (m_IsTickingActors)
    {
        m_PendingTickAdd.Emplace(actor);
    }
    else
    {
        m_TickActors.Emplace(actor);
    }

    actor->m_bInTickList = true;
}

void Scene::UnregisterTickActor(Actor* actor)
{
    if (!actor || !actor->m_bInTickList)
        return;

    if (m_IsTickingActors)
    {
        m_PendingTickRemove.Emplace(actor);
    }
    else
    {
        for (uint32 i = 0; i < m_TickActors.Num(); ++i)
        {
            if (m_TickActors[i] == actor)
            {
                m_TickActors.RemoveAt(i);
                break;
            }
        }
    }

    actor->m_bInTickList = false;
}

void Scene::Clear()
{
    // destroy actors (unique_ptr array owns them)
    for (auto& a : m_Actors)
    {
        if (a) DestroyActor(a.Get());
    }
    FlushPendingActorDestroy();
    m_Actors.Clear();
    m_ActorsMap.Clear();
    m_TickActors.Clear();
    m_PendingTickAdd.Clear();
    m_PendingTickRemove.Clear();
    m_PrePhysicsTickActors.Clear();
    m_PostPhysicsTickActors.Clear();
    m_PostUpdateTickActors.Clear();
    m_bHasBegunPlay = false;

    m_Registry.clear();
}

bool Scene::Deserialize(const String& filename)
{
    Clear();

    if (!m_Serializer.LoadFromFile(filename))
        return false;

    // Scene root
    m_Serializer.BeginObjectRead("Scene");

    m_Serializer.BeginObjectRead("GameMode");
    {
        String gameModeTypeName;
        if (m_Serializer.Read("Type", gameModeTypeName) && m_World)
        {
            const TypeInfo* gameModeType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(gameModeTypeName);
            if (gameModeType && gameModeType->IsA(GameMode::StaticType()) && gameModeType->CreateInstance)
            {
                std::unique_ptr<GameMode> gameMode(static_cast<GameMode*>(gameModeType->CreateInstance()));
                m_Serializer.DeserializeType(gameModeType, gameMode.get());
                m_World->SetGameMode(std::move(gameMode));
            }
        }
    }
    m_Serializer.EndObjectRead();

    // Actors array
    m_Serializer.BeginArrayRead("Actors");
    const size_t actorCount = m_Serializer.GetArraySize();

    for (size_t i = 0; i < actorCount; ++i)
    {
        m_Serializer.BeginArrayElementRead(i);

        YAML::Node actorElem = m_Serializer.Current();
        if (!actorElem || !actorElem.IsMap())
        {
            m_Serializer.EndArrayElementRead();
            continue;
        }

        ActorTemplateSerializer::DeserializeActorTemplate(*this, m_Serializer, actorElem);

        m_Serializer.EndArrayElementRead();
    }

    m_Serializer.EndArrayRead();
    m_Serializer.EndObjectRead();

    // rebuild world transforms
    UpdateTransforms();
    return true;
}

Actor* Scene::SpawnActorFromPrefab(const PrefabAsset& prefab)
{
    if (prefab.m_TemplateYaml.length() == 0)
        return nullptr;

    return ActorTemplateSerializer::SpawnActorTemplateFromString(*this, prefab.m_TemplateYaml);
}






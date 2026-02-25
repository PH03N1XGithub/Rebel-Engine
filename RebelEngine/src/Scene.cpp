// Scene.cpp
#include "EnginePch.h"
#include "Scene.h"

// ---------- BeginPlay ----------

void Scene::BeginPlay()
{
    // Call BeginPlay on all existing actors once
    for (auto& actorPtr : m_Actors)
    {
        actorPtr->BeginPlay();
    }
}

// ---------- DestroyActor ----------
DEFINE_LOG_CATEGORY(actorLog)
Actor& Scene::SpawnActor(const TypeInfo* type)
{
    CHECK_MSG(type, "SpawnActor: type is null!");
    CHECK_MSG(type->IsA(Actor::StaticType()), "SpawnActor: type is not an Actor!");
    CHECK_MSG(type->CreateInstance != nullptr, "SpawnActor: type is abstract (no factory)!");

    // 1) create entity
    entt::entity e = m_Registry.create();

    // ✅ CALL the factory
    Actor* actor = static_cast<Actor*>(type->CreateInstance());
    CHECK(actor);

    actor->Init(e, this);

    actor->AddComponent<IDComponent>();
    actor->AddComponent<ActorTagComponent>();
    actor->AddComponent<SceneComponent>();
    actor->AddComponent<NameComponent>();

    m_Actors.Emplace(RUniquePtr<Actor>(actor));
    m_ActorsMap.Add(e, actor);
    

    if (actor->CanEverTick() && actor->IsTickEnabled())
        RegisterTickActor(actor);
    
    
    RB_LOG(actorLog, info, "Spawn requested type={}, created dynamic type={}",
       type->Name, actor->GetType()->Name)


    return *actor;

}

void Scene::DestroyActor(Actor* actor)
{
    if (!actor || actor->bPendingDestroy)
        return;

    actor->bPendingDestroy = true;

    // Remove from tick list safely
    UnregisterTickActor(actor);

    m_PendingDestroyActors.Emplace(actor);
}

void Scene::FlushPendingActorDestroy()
{
    for (Actor* actor : m_PendingDestroyActors)
    {
        // 1️⃣ Destroy object components (ECS + C++)
        actor->DestroyAllComponents();

        entt::entity e = actor->GetHandle();

        // 2️⃣ Remove lookup
        if (e != entt::null)
            m_ActorsMap.Remove(e);

        // 3️⃣ Destroy actor ECS entity
        if (e != entt::null)
            m_Registry.destroy(e);

        // 4️⃣ Delete actor object
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

void Scene::Tick(float dt)
{
    m_IsTickingActors = true;

    const uint64 count = m_TickActors.Num();
    for (uint64 i = 0; i < count; ++i)
    {
        Actor* actor = m_TickActors[i];
        actor->Tick(dt);
    }

    m_IsTickingActors = false;

    // game logic, actor ticking, etc...
    // ...

    // After all movement / logic:
    UpdateTransforms();

    // Apply pending removals
    for (Actor* a : m_PendingTickRemove)
    {
        for (uint32 i = 0; i < m_TickActors.Num(); ++i)
        {
            if (m_TickActors[i] == a)
            {
                m_TickActors.EraseAtSwap(i);
                break;
            }
        }
        a->m_bInTickList = false;
    }
    m_PendingTickRemove.Clear();

    // Apply pending additions
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

    // ---------- (Later) ECS systems ----------
    // Here you run your entt systems:
    // - movement
    // - camera
    // - rendering, etc.
}

void Scene::UpdateTransforms()
{
    auto& reg = m_Registry;
    auto view = reg.view<SceneComponent*>();

    // First: update all roots (no parent)
    for (auto entity : view)
    {
        auto& sc = view.get<SceneComponent*>(entity);
 
        // Root: parentWorld = identity
        UpdateTransformRecursive(entity, Mat4(1.0f));
        
    }
}

void Scene::UpdateTransformRecursive(entt::entity entity, const Mat4& parentWorld)
{
    auto& reg = m_Registry;
    auto& sc  = reg.get<SceneComponent*>(entity);

    // Build local from Position / RotationQuat / Scale
    Mat4 local = sc->GetLocalTransform();

    // World = ParentWorld * Local
    sc->WorldTransform = parentWorld * local;
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
                m_TickActors.EraseAtSwap(i);
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

    m_Registry.clear();
}

bool Scene::Deserialize(const String& filename)
{
    Clear();

    if (!m_Serializer.LoadFromFile(filename))
        return false;

    // Scene root
    m_Serializer.BeginObjectRead("Scene");

    // Actors array
    m_Serializer.BeginArrayRead("Actors");
    const size_t actorCount = m_Serializer.GetArraySize();

    for (size_t i = 0; i < actorCount; ++i)
    {
        m_Serializer.BeginArrayElementRead(i);

        const YAML::Node& actorElem = m_Serializer.Current();
        if (!actorElem || !actorElem.IsMap())
        {
            m_Serializer.EndArrayElementRead();
            continue;
        }

        // Find actor type key: first key that is NOT "Components"
        String actorTypeName = "Actor";
        YAML::Node actorDataNode;

        for (auto it = actorElem.begin(); it != actorElem.end(); ++it)
        {
            const String key = it->first.as<String>();
            if (key == "Components")
                continue;

            actorTypeName = key;
            actorDataNode = it->second; // could be ~ (null) or a map
            break;
        }

        const auto* actorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(actorTypeName);
        if (!actorType || !actorType->IsA(Actor::StaticType()))
            actorType = Actor::StaticType();

        Actor& actor = SpawnActor(actorType);

        // Actor fields (if any)
        if (actorDataNode && actorDataNode.IsMap())
        {
            m_Serializer.PushNode(actorDataNode);
            m_Serializer.DeserializeTypeRecursive(actorType, &actor);
            m_Serializer.PopNode();
        }

        // Components
        const YAML::Node compsNode = actorElem["Components"];
        if (compsNode && compsNode.IsMap())
        {
            for (auto it = compsNode.begin(); it != compsNode.end(); ++it)
            {
                const String compName = it->first.as<String>();
                const YAML::Node compNode = it->second;

                const auto* compInfo = ComponentRegistry::Get().FindByName(compName);
                if (!compInfo || !compInfo->Type || !compInfo->AddFn || !compInfo->GetFn)
                    continue;

                entt::entity e = actor.GetHandle();

                // Ensure component exists
                if (!compInfo->HasFn || !compInfo->HasFn(actor))
                    compInfo->AddFn(actor);

                void* compPtr = compInfo->GetFn(actor);
                if (!compPtr)
                    continue;

                // Deserialize component data
                if (compNode && compNode.IsMap())
                {
                    m_Serializer.PushNode(compNode);
                    m_Serializer.DeserializeTypeRecursive(compInfo->Type, compPtr);
                    m_Serializer.PopNode();
                }

                // IMPORTANT: fix SceneComponent quaternion sync
                if (compInfo->Type->IsA(SceneComponent::StaticType()))
                {
                    auto* sc = reinterpret_cast<SceneComponent*>(compPtr);
                    sc->SetRotationEuler(sc->Rotation);
                }
            }
        }

        m_Serializer.EndArrayElementRead();
    }

    m_Serializer.EndArrayRead();
    m_Serializer.EndObjectRead();

    // rebuild world transforms
    UpdateTransforms();
    return true;
}


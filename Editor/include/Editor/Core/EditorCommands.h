#pragma once

#include "Editor/Core/EditorCommandSystem.h"
#include "Editor/Core/EditorSelection.h"
#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Scene/ActorTemplateSerializer.h"
#include "Core/AssetPtrBase.h"

#include <ThirdParty/entt.h>
#include <glm/gtc/quaternion.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <variant>

struct LocalTransformSnapshot
{
    Vector3 Position{ 0.0f };
    glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    Vector3 Scale{ 1.0f };
};

class TransformSceneComponentCommand final : public IEditorCommand
{
public:
    TransformSceneComponentCommand(
        SceneComponent* target,
        const LocalTransformSnapshot& before,
        const LocalTransformSnapshot& after,
        const char* label = "Transform");

    static LocalTransformSnapshot Capture(const SceneComponent& component);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override;

private:
    static bool Apply(SceneComponent* target, const LocalTransformSnapshot& transform);

private:
    SceneComponent* m_Target = nullptr;
    LocalTransformSnapshot m_Before;
    LocalTransformSnapshot m_After;
    std::string m_Label;
};

class ReparentActorCommand final : public IEditorCommand
{
public:
    ReparentActorCommand(Actor* childActor, Actor* newParentActor);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override;

private:
    bool Apply(entt::entity parentActorHandle);

private:
    Scene* m_Scene = nullptr;
    entt::entity m_ChildActor = entt::null;
    entt::entity m_OldParentActor = entt::null;
    entt::entity m_NewParentActor = entt::null;
    std::string m_Label = "Reparent Actor";
};

class SpawnActorCommand final : public IEditorCommand
{
public:
    SpawnActorCommand(Scene* scene, const Rebel::Core::Reflection::TypeInfo* actorType, String desiredName = "");

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Spawn Actor"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveSpawnedActor(Scene& scene) const;

private:
    Scene* m_Scene = nullptr;
    const Rebel::Core::Reflection::TypeInfo* m_ActorType = nullptr;
    entt::entity m_SpawnedActor = entt::null;
    String m_DesiredName;
    String m_SpawnedName;
};

class SpawnPawnControllerCommand final : public IEditorCommand
{
public:
    explicit SpawnPawnControllerCommand(Scene* scene);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Place Controller"; }

private:
    Scene* ResolveScene(EditorContext& context) const;

private:
    Scene* m_Scene = nullptr;
    entt::entity m_PawnHandle = entt::null;
    entt::entity m_ControllerHandle = entt::null;
};

class DeleteActorCommand final : public IEditorCommand
{
public:
    explicit DeleteActorCommand(Actor* actor);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Delete Actor"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveTargetActor(Scene& scene) const;
    static Actor* FindActorByName(Scene& scene, const String& actorName);
    static String MakeSnapshotPath();

private:
    Scene* m_Scene = nullptr;
    entt::entity m_TargetActor = entt::null;
    String m_TargetName;
    String m_SnapshotPath;
};

class DuplicateActorCommand final : public IEditorCommand
{
public:
    explicit DuplicateActorCommand(Actor* sourceActor);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Duplicate Actor"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveSpawnedActor(Scene& scene) const;
    static String MakeUniqueActorName(Scene& scene, const String& baseName);

private:
    Scene* m_Scene = nullptr;
    entt::entity m_SourceActor = entt::null;
    entt::entity m_SourceParentActor = entt::null;
    entt::entity m_SpawnedActor = entt::null;
    String m_SourceName;
    String m_SpawnedName;
    String m_TemplateYaml;
    Mat4 m_SourceTransform{ 1.0f };
};

class AddComponentCommand final : public IEditorCommand
{
public:
    AddComponentCommand(Actor* actor, const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Add Component"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveActor(Scene& scene) const;

private:
    Scene* m_Scene = nullptr;
    entt::entity m_ActorHandle = entt::null;
    EntityComponent* m_AddedComponent = nullptr;
    String m_ComponentName;
};

class RemoveComponentCommand final : public IEditorCommand
{
public:
    RemoveComponentCommand(
        Actor* actor,
        const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo,
        EntityComponent* componentInstance = nullptr);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Remove Component"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveActor(Scene& scene) const;
    static String MakeSnapshotPath();

private:
    Scene* m_Scene = nullptr;
    entt::entity m_ActorHandle = entt::null;
    EntityComponent* m_ComponentInstance = nullptr;
    String m_ActorName;
    String m_ComponentName;
    String m_SnapshotPath;
};

class DuplicateComponentCommand final : public IEditorCommand
{
public:
    DuplicateComponentCommand(
        Actor* actor,
        const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo,
        EntityComponent* componentInstance = nullptr);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Duplicate Component"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveActor(Scene& scene) const;
    static bool CopyReflectedProperties(
        const Rebel::Core::Reflection::TypeInfo* typeInfo,
        const void* source,
        void* destination);
    static String MakeSnapshotPath();

private:
    Scene* m_Scene = nullptr;
    entt::entity m_ActorHandle = entt::null;
    EntityComponent* m_ComponentInstance = nullptr;
    String m_ActorName;
    String m_ComponentName;
    String m_SnapshotPath;
};

class RenameActorCommand final : public IEditorCommand
{
public:
    RenameActorCommand(Actor* actor, String beforeName, String afterName);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Rename Actor"; }

private:
    Scene* ResolveScene(EditorContext& context) const;
    Actor* ResolveActor(Scene& scene) const;

private:
    Scene* m_Scene = nullptr;
    entt::entity m_ActorHandle = entt::null;
    String m_BeforeName;
    String m_AfterName;
};

class SetPropertyCommand final : public IEditorCommand
{
public:
    using PropertyValue = std::variant<std::monostate, int32, uint64, float, bool, String, Vector3, AssetHandle, MaterialHandle, const Rebel::Core::Reflection::TypeInfo*>;

    SetPropertyCommand(
        void* object,
        const Rebel::Core::Reflection::PropertyInfo& property,
        const Rebel::Core::Reflection::TypeInfo* ownerType,
        PropertyValue before,
        PropertyValue after);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Set Property"; }

    static PropertyValue Capture(void* object, const Rebel::Core::Reflection::PropertyInfo& property);

private:
    bool Apply(const PropertyValue& value) const;
    bool IsSceneRotationProperty() const;

private:
    void* m_Object = nullptr;
    Rebel::Core::Reflection::PropertyInfo m_Property;
    const Rebel::Core::Reflection::TypeInfo* m_OwnerType = nullptr;
    PropertyValue m_Before;
    PropertyValue m_After;
};

class AssignAssetCommand final : public IEditorCommand
{
public:
    AssignAssetCommand(AssetPtrBase* target, AssetHandle before, AssetHandle after);

    bool Execute(EditorContext& context) override;
    void Undo(EditorContext& context) override;
    const char* GetLabel() const override { return "Assign Asset"; }

private:
    AssetPtrBase* m_Target = nullptr;
    AssetHandle m_Before = 0;
    AssetHandle m_After = 0;
};

inline TransformSceneComponentCommand::TransformSceneComponentCommand(
    SceneComponent* target,
    const LocalTransformSnapshot& before,
    const LocalTransformSnapshot& after,
    const char* label)
    : m_Target(target)
    , m_Before(before)
    , m_After(after)
    , m_Label(label ? label : "Transform")
{
}

inline LocalTransformSnapshot TransformSceneComponentCommand::Capture(const SceneComponent& component)
{
    LocalTransformSnapshot snapshot{};
    snapshot.Position = component.GetPosition();
    snapshot.Rotation = component.GetRotationQuat();
    snapshot.Scale = component.GetScale();
    return snapshot;
}

inline bool TransformSceneComponentCommand::Execute(EditorContext& context)
{
    (void)context;
    return Apply(m_Target, m_After);
}

inline void TransformSceneComponentCommand::Undo(EditorContext& context)
{
    (void)context;
    Apply(m_Target, m_Before);
}

inline const char* TransformSceneComponentCommand::GetLabel() const
{
    return m_Label.c_str();
}

inline bool TransformSceneComponentCommand::Apply(SceneComponent* target, const LocalTransformSnapshot& transform)
{
    if (!target)
        return false;

    Actor* owner = target->GetOwner();
    if (!owner || !owner->IsValid())
        return false;

    target->SetPosition(transform.Position);
    target->SetRotationQuat(transform.Rotation);
    target->SetScale(transform.Scale);
    return true;
}

inline ReparentActorCommand::ReparentActorCommand(Actor* childActor, Actor* newParentActor)
{
    if (!childActor || !childActor->IsValid())
        return;

    m_Scene = childActor->GetScene();
    if (!m_Scene)
        return;

    m_ChildActor = childActor->GetHandle();
    m_NewParentActor = (newParentActor && newParentActor->IsValid()) ? newParentActor->GetHandle() : entt::null;

    SceneComponent* childRoot = childActor->GetRootComponent();
    if (childRoot && childRoot->GetParent() && childRoot->GetParent()->GetOwner())
        m_OldParentActor = childRoot->GetParent()->GetOwner()->GetHandle();
}

inline bool ReparentActorCommand::Execute(EditorContext& context)
{
    (void)context;
    return Apply(m_NewParentActor);
}

inline void ReparentActorCommand::Undo(EditorContext& context)
{
    (void)context;
    Apply(m_OldParentActor);
}

inline const char* ReparentActorCommand::GetLabel() const
{
    return m_Label.c_str();
}

inline bool ReparentActorCommand::Apply(entt::entity parentActorHandle)
{
    if (!m_Scene)
        return false;

    Actor* childActor = m_Scene->GetActor(m_ChildActor);
    if (!childActor || !childActor->IsValid())
        return false;

    SceneComponent* childRoot = childActor->GetRootComponent();
    if (!childRoot)
        return false;

    SceneComponent* parentRoot = nullptr;
    if (parentActorHandle != entt::null)
    {
        Actor* parentActor = m_Scene->GetActor(parentActorHandle);
        if (!parentActor || !parentActor->IsValid() || parentActor == childActor)
            return false;

        parentRoot = parentActor->GetRootComponent();
        if (!parentRoot)
            return false;
    }

    return childRoot->AttachTo(parentRoot, true);
}

inline SpawnActorCommand::SpawnActorCommand(Scene* scene, const Rebel::Core::Reflection::TypeInfo* actorType, String desiredName)
    : m_Scene(scene)
    , m_ActorType(actorType)
    , m_DesiredName(std::move(desiredName))
{
}

inline Scene* SpawnActorCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* SpawnActorCommand::ResolveSpawnedActor(Scene& scene) const
{
    if (m_SpawnedActor != entt::null)
    {
        Actor* actor = scene.GetActor(m_SpawnedActor);
        if (actor && actor->IsValid())
            return actor;
    }

    if (m_SpawnedName.length() > 0)
    {
        for (const auto& actorPtr : scene.GetActors())
        {
            Actor* actor = actorPtr.Get();
            if (actor && actor->IsValid() && actor->GetName() == m_SpawnedName)
                return actor;
        }
    }

    return nullptr;
}

inline bool SpawnActorCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene || !m_ActorType)
        return false;

    Actor& actor = scene->SpawnActor(m_ActorType);
    if (m_DesiredName.length() > 0)
        actor.SetName(m_DesiredName);

    m_SpawnedActor = actor.GetHandle();
    m_SpawnedName = actor.GetName();

    if (context.Selection)
        context.Selection->SetSingleActor(&actor);

    return true;
}

inline void SpawnActorCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return;

    Actor* actor = ResolveSpawnedActor(*scene);
    if (!actor)
        return;

    if (context.Selection)
        context.Selection->RemoveActor(actor);

    actor->Destroy();
    scene->FlushPendingActorDestroy();
}

inline SpawnPawnControllerCommand::SpawnPawnControllerCommand(Scene* scene)
    : m_Scene(scene)
{
}

inline Scene* SpawnPawnControllerCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline bool SpawnPawnControllerCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Pawn& pawn = scene->SpawnActor<Pawn>();
    Controller& controller = scene->SpawnActor<Controller>();

    controller.SetTickPriority(-100);
    pawn.SetTickPriority(0);
    controller.Possess(&pawn);

    m_PawnHandle = pawn.GetHandle();
    m_ControllerHandle = controller.GetHandle();

    if (context.Selection)
        context.Selection->SetSingleActor(&pawn);

    return true;
}

inline void SpawnPawnControllerCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return;

    if (Actor* controller = scene->GetActor(m_ControllerHandle))
        controller->Destroy();

    if (Actor* pawn = scene->GetActor(m_PawnHandle))
        pawn->Destroy();

    scene->FlushPendingActorDestroy();

    if (context.Selection)
        context.Selection->SyncWithScene(scene);
}

inline DeleteActorCommand::DeleteActorCommand(Actor* actor)
{
    if (!actor || !actor->IsValid())
        return;

    m_Scene = actor->GetScene();
    m_TargetActor = actor->GetHandle();
    m_TargetName = actor->GetName();
}

inline Scene* DeleteActorCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* DeleteActorCommand::FindActorByName(Scene& scene, const String& actorName)
{
    if (actorName.length() == 0)
        return nullptr;

    for (const auto& actorPtr : scene.GetActors())
    {
        Actor* actor = actorPtr.Get();
        if (actor && actor->IsValid() && actor->GetName() == actorName)
            return actor;
    }

    return nullptr;
}

inline Actor* DeleteActorCommand::ResolveTargetActor(Scene& scene) const
{
    Actor* actor = scene.GetActor(m_TargetActor);
    if (actor && actor->IsValid())
        return actor;

    return FindActorByName(scene, m_TargetName);
}

inline String DeleteActorCommand::MakeSnapshotPath()
{
    using Clock = std::chrono::high_resolution_clock;
    const auto ticks = Clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("rebel_editor_delete_" + std::to_string(ticks) + ".ryml");
    return String(path.string().c_str());
}

inline bool DeleteActorCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Actor* actor = ResolveTargetActor(*scene);
    if (!actor)
        return false;

    m_TargetActor = actor->GetHandle();
    m_TargetName = actor->GetName();

    if (m_SnapshotPath.length() == 0)
    {
        m_SnapshotPath = MakeSnapshotPath();
        scene->Serialize(m_SnapshotPath);
    }

    if (context.Selection)
        context.Selection->RemoveActor(actor);

    actor->Destroy();
    scene->FlushPendingActorDestroy();
    return true;
}

inline void DeleteActorCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene || m_SnapshotPath.length() == 0)
        return;

    if (!scene->Deserialize(m_SnapshotPath))
        return;

    if (!context.Selection)
        return;

    Actor* restored = FindActorByName(*scene, m_TargetName);
    if (restored)
        context.Selection->SetSingleActor(restored);
    else
        context.Selection->Clear();
}

inline DuplicateActorCommand::DuplicateActorCommand(Actor* sourceActor)
{
    if (!sourceActor || !sourceActor->IsValid())
        return;

    m_Scene = sourceActor->GetScene();
    m_SourceActor = sourceActor->GetHandle();
    m_SourceName = sourceActor->GetName();
    m_SourceTransform = sourceActor->GetActorTransform();

    if (SceneComponent* root = sourceActor->GetRootComponent())
    {
        if (SceneComponent* parentRoot = root->GetParent())
        {
            if (Actor* parentActor = parentRoot->GetOwner())
                m_SourceParentActor = parentActor->GetHandle();
        }
    }

    ActorTemplateSerializer::SerializeActorTemplateToString(
        *sourceActor,
        m_TemplateYaml,
        { false });
}

inline Scene* DuplicateActorCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* DuplicateActorCommand::ResolveSpawnedActor(Scene& scene) const
{
    if (m_SpawnedActor != entt::null)
    {
        Actor* actor = scene.GetActor(m_SpawnedActor);
        if (actor && actor->IsValid())
            return actor;
    }

    if (m_SpawnedName.length() > 0)
    {
        for (const auto& actorPtr : scene.GetActors())
        {
            Actor* actor = actorPtr.Get();
            if (actor && actor->IsValid() && actor->GetName() == m_SpawnedName)
                return actor;
        }
    }

    return nullptr;
}

inline String DuplicateActorCommand::MakeUniqueActorName(Scene& scene, const String& baseName)
{
    const String cleanBase = baseName.length() > 0 ? baseName : String("Actor");
    auto nameExists = [&scene](const String& candidate)
    {
        for (const auto& actorPtr : scene.GetActors())
        {
            Actor* actor = actorPtr.Get();
            if (actor && actor->IsValid() && actor->GetName() == candidate)
                return true;
        }

        return false;
    };

    const String copyBase = cleanBase + "_Copy";
    if (!nameExists(copyBase))
        return copyBase;

    for (int index = 1; index < 10000; ++index)
    {
        const String candidate = copyBase + "_" + String(std::to_string(index).c_str());
        if (!nameExists(candidate))
            return candidate;
    }

    return copyBase + "_9999";
}

inline bool DuplicateActorCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene || m_TemplateYaml.length() == 0)
        return false;

    Actor* spawned = ActorTemplateSerializer::SpawnActorTemplateFromString(*scene, m_TemplateYaml);
    if (!spawned || !spawned->IsValid())
        return false;

    m_SpawnedActor = spawned->GetHandle();
    m_SpawnedName = MakeUniqueActorName(*scene, m_SourceName);
    spawned->SetName(m_SpawnedName);

    if (SceneComponent* root = spawned->GetRootComponent())
    {
        SceneComponent* parentRoot = nullptr;
        if (m_SourceParentActor != entt::null)
        {
            if (Actor* parentActor = scene->GetActor(m_SourceParentActor))
                parentRoot = parentActor->GetRootComponent();
        }

        root->AttachTo(parentRoot, true);
    }

    spawned->SetActorTransform(m_SourceTransform);
    scene->FinalizeDeferredActorSpawn(*spawned);

    if (context.Selection)
        context.Selection->SetSingleActor(spawned);

    return true;
}

inline void DuplicateActorCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return;

    Actor* actor = ResolveSpawnedActor(*scene);
    if (!actor)
        return;

    if (context.Selection)
        context.Selection->RemoveActor(actor);

    actor->Destroy();
    scene->FlushPendingActorDestroy();
}

inline AddComponentCommand::AddComponentCommand(Actor* actor, const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo)
{
    if (!actor || !actor->IsValid() || !componentInfo)
        return;

    m_Scene = actor->GetScene();
    m_ActorHandle = actor->GetHandle();
    m_ComponentName = componentInfo->Name;
}

inline Scene* AddComponentCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* AddComponentCommand::ResolveActor(Scene& scene) const
{
    Actor* actor = scene.GetActor(m_ActorHandle);
    if (!actor || !actor->IsValid())
        return nullptr;

    return actor;
}

inline bool AddComponentCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return false;

    const Rebel::Core::Reflection::ComponentTypeInfo* info =
        Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(m_ComponentName);
    if (!info || !info->AddFn || !info->Type)
        return false;

    const bool isObjectComponent = info->Type->IsA(EntityComponent::StaticType());
    TArray<EntityComponent*> beforeComponents;
    if (isObjectComponent)
    {
        for (const auto& componentPtr : actor->GetObjectComponents())
        {
            if (EntityComponent* component = componentPtr.Get())
                beforeComponents.Add(component);
        }
    }

    if (isObjectComponent || !info->HasFn || !info->HasFn(*actor))
        info->AddFn(*actor);

    if (context.Selection)
    {
        context.Selection->SelectedComponentType = info->Type;
        context.Selection->SelectedComponent = nullptr;

        if (isObjectComponent)
        {
            for (const auto& componentPtr : actor->GetObjectComponents())
            {
                EntityComponent* component = componentPtr.Get();
                if (!component || component->GetType() != info->Type)
                    continue;

                bool existedBefore = false;
                for (EntityComponent* beforeComponent : beforeComponents)
                {
                    if (beforeComponent == component)
                    {
                        existedBefore = true;
                        break;
                    }
                }

                if (!existedBefore)
                {
                    m_AddedComponent = component;
                    context.Selection->SelectedComponent = component;
                    break;
                }
            }
        }
    }

    return true;
}

inline void AddComponentCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return;

    const Rebel::Core::Reflection::ComponentTypeInfo* info =
        Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(m_ComponentName);
    if (!info || !info->RemoveFn)
        return;

    const bool isObjectComponent = info->Type && info->Type->IsA(EntityComponent::StaticType());
    if (isObjectComponent && m_AddedComponent)
    {
        actor->RemoveObjectComponentInstance(m_AddedComponent);
        m_AddedComponent = nullptr;
    }
    else if (info->HasFn && info->HasFn(*actor))
    {
        info->RemoveFn(*actor);
    }

    if (context.Selection && context.Selection->SelectedComponentType == info->Type)
    {
        context.Selection->SelectedComponent = nullptr;
        context.Selection->SelectedComponentType = nullptr;
    }
}

inline RemoveComponentCommand::RemoveComponentCommand(
    Actor* actor,
    const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo,
    EntityComponent* componentInstance)
{
    if (!actor || !actor->IsValid() || !componentInfo)
        return;

    m_Scene = actor->GetScene();
    m_ActorHandle = actor->GetHandle();
    m_ComponentInstance = componentInstance;
    m_ActorName = actor->GetName();
    m_ComponentName = componentInfo->Name;
}

inline Scene* RemoveComponentCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* RemoveComponentCommand::ResolveActor(Scene& scene) const
{
    Actor* actor = scene.GetActor(m_ActorHandle);
    if (actor && actor->IsValid())
        return actor;

    for (const auto& actorPtr : scene.GetActors())
    {
        Actor* candidate = actorPtr.Get();
        if (candidate && candidate->IsValid() && candidate->GetName() == m_ActorName)
            return candidate;
    }

    return nullptr;
}

inline String RemoveComponentCommand::MakeSnapshotPath()
{
    using Clock = std::chrono::high_resolution_clock;
    const auto ticks = Clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("rebel_editor_component_remove_" + std::to_string(ticks) + ".ryml");
    return String(path.string().c_str());
}

inline bool RemoveComponentCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return false;

    const Rebel::Core::Reflection::ComponentTypeInfo* info =
        Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(m_ComponentName);
    if (!info || !info->RemoveFn || !info->HasFn || !info->HasFn(*actor))
        return false;

    if (m_SnapshotPath.length() == 0)
    {
        m_SnapshotPath = MakeSnapshotPath();
        scene->Serialize(m_SnapshotPath);
    }

    if (m_ComponentInstance)
    {
        if (!actor->RemoveObjectComponentInstance(m_ComponentInstance))
            return false;
    }
    else
    {
        info->RemoveFn(*actor);
    }

    if (context.Selection)
    {
        context.Selection->SelectedComponent = nullptr;
        if (context.Selection->SelectedComponentType == info->Type)
            context.Selection->SelectedComponentType = nullptr;
    }

    return true;
}

inline void RemoveComponentCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene || m_SnapshotPath.length() == 0)
        return;

    if (!scene->Deserialize(m_SnapshotPath))
        return;

    if (!context.Selection)
        return;

    Actor* restored = ResolveActor(*scene);
    if (restored)
    {
        context.Selection->SetSingleActor(restored);
        if (const Rebel::Core::Reflection::ComponentTypeInfo* info =
                Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(m_ComponentName))
            context.Selection->SelectedComponentType = info->Type;
    }
    else
    {
        context.Selection->Clear();
    }
}

inline DuplicateComponentCommand::DuplicateComponentCommand(
    Actor* actor,
    const Rebel::Core::Reflection::ComponentTypeInfo* componentInfo,
    EntityComponent* componentInstance)
{
    if (!actor || !actor->IsValid() || !componentInfo)
        return;

    m_Scene = actor->GetScene();
    m_ActorHandle = actor->GetHandle();
    m_ComponentInstance = componentInstance;
    m_ActorName = actor->GetName();
    m_ComponentName = componentInfo->Name;
}

inline Scene* DuplicateComponentCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* DuplicateComponentCommand::ResolveActor(Scene& scene) const
{
    Actor* actor = scene.GetActor(m_ActorHandle);
    if (actor && actor->IsValid())
        return actor;

    for (const auto& actorPtr : scene.GetActors())
    {
        Actor* candidate = actorPtr.Get();
        if (candidate && candidate->IsValid() && candidate->GetName() == m_ActorName)
            return candidate;
    }

    return nullptr;
}

inline bool DuplicateComponentCommand::CopyReflectedProperties(
    const Rebel::Core::Reflection::TypeInfo* typeInfo,
    const void* source,
    void* destination)
{
    if (!typeInfo || !source || !destination)
        return false;

    if (typeInfo->Super)
        CopyReflectedProperties(typeInfo->Super, source, destination);

    using namespace Rebel::Core::Reflection;
    for (const PropertyInfo& prop : typeInfo->Properties)
    {
        if (HasFlag(prop.Flags, EPropertyFlags::Transient))
            continue;

        const uint8* sourceBytes = reinterpret_cast<const uint8*>(source) + prop.Offset;
        uint8* destinationBytes = reinterpret_cast<uint8*>(destination) + prop.Offset;

        switch (prop.Type)
        {
        case EPropertyType::String:
            *reinterpret_cast<String*>(destinationBytes) = *reinterpret_cast<const String*>(sourceBytes);
            break;
        case EPropertyType::Asset:
        {
            auto* destinationAsset = reinterpret_cast<AssetPtrBase*>(destinationBytes);
            const auto* sourceAsset = reinterpret_cast<const AssetPtrBase*>(sourceBytes);
            if (destinationAsset && sourceAsset)
                destinationAsset->SetHandle(sourceAsset->GetHandle());
            break;
        }
        case EPropertyType::Class:
        case EPropertyType::Int8:
        case EPropertyType::UInt8:
        case EPropertyType::Int16:
        case EPropertyType::UInt16:
        case EPropertyType::Int32:
        case EPropertyType::UInt32:
        case EPropertyType::Int64:
        case EPropertyType::UInt64:
        case EPropertyType::Float:
        case EPropertyType::Double:
        case EPropertyType::Bool:
        case EPropertyType::Enum:
        case EPropertyType::Vector3:
        case EPropertyType::MaterialHandle:
            std::memcpy(destinationBytes, sourceBytes, prop.Size);
            break;
        case EPropertyType::Unknown:
        default:
            break;
        }
    }

    return true;
}

inline String DuplicateComponentCommand::MakeSnapshotPath()
{
    using Clock = std::chrono::high_resolution_clock;
    const auto ticks = Clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("rebel_editor_component_duplicate_" + std::to_string(ticks) + ".ryml");
    return String(path.string().c_str());
}

inline bool DuplicateComponentCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return false;

    const Rebel::Core::Reflection::ComponentTypeInfo* info =
        Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(m_ComponentName);
    if (!info || !info->AddFn || !info->Type || !info->Type->IsA(EntityComponent::StaticType()))
        return false;

    EntityComponent* source = m_ComponentInstance;
    if (!source)
    {
        if (!info->GetFn || !info->HasFn || !info->HasFn(*actor))
            return false;

        source = static_cast<EntityComponent*>(info->GetFn(*actor));
    }

    if (!source || source->GetOwner() != actor)
        return false;

    TArray<EntityComponent*> beforeComponents;
    for (const auto& componentPtr : actor->GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (component && component->GetType() == info->Type)
            beforeComponents.Add(component);
    }

    if (m_SnapshotPath.length() == 0)
    {
        m_SnapshotPath = MakeSnapshotPath();
        scene->Serialize(m_SnapshotPath);
    }

    info->AddFn(*actor);

    EntityComponent* duplicate = nullptr;
    for (const auto& componentPtr : actor->GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component || component->GetType() != info->Type)
            continue;

        bool existedBefore = false;
        for (EntityComponent* beforeComponent : beforeComponents)
        {
            if (beforeComponent == component)
            {
                existedBefore = true;
                break;
            }
        }

        if (!existedBefore)
        {
            duplicate = component;
            break;
        }
    }

    if (!duplicate)
        return false;

    CopyReflectedProperties(info->Type, source, duplicate);
    duplicate->SetEditorName(actor->MakeUniqueComponentEditorName(source->GetEditorName(), duplicate));
    if (SceneComponent* duplicateSceneComponent = dynamic_cast<SceneComponent*>(duplicate))
        duplicateSceneComponent->SetRotationEuler(duplicateSceneComponent->GetRotationEuler());

    if (context.Selection)
    {
        context.Selection->SelectedComponent = duplicate;
        context.Selection->SelectedComponentType = info->Type;
    }

    return true;
}

inline void DuplicateComponentCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene || m_SnapshotPath.length() == 0)
        return;

    if (!scene->Deserialize(m_SnapshotPath))
        return;

    if (!context.Selection)
        return;

    Actor* restored = ResolveActor(*scene);
    if (restored)
        context.Selection->SetSingleActor(restored);
    else
        context.Selection->Clear();
}

inline RenameActorCommand::RenameActorCommand(Actor* actor, String beforeName, String afterName)
    : m_BeforeName(std::move(beforeName))
    , m_AfterName(std::move(afterName))
{
    if (!actor || !actor->IsValid())
        return;

    m_Scene = actor->GetScene();
    m_ActorHandle = actor->GetHandle();
}

inline Scene* RenameActorCommand::ResolveScene(EditorContext& context) const
{
    if (context.ActiveScene)
        return context.ActiveScene;
    return m_Scene;
}

inline Actor* RenameActorCommand::ResolveActor(Scene& scene) const
{
    Actor* actor = scene.GetActor(m_ActorHandle);
    if (!actor || !actor->IsValid())
        return nullptr;
    return actor;
}

inline bool RenameActorCommand::Execute(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return false;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return false;

    if (m_BeforeName.length() == 0)
        m_BeforeName = actor->GetName();

    actor->SetName(m_AfterName);
    return true;
}

inline void RenameActorCommand::Undo(EditorContext& context)
{
    Scene* scene = ResolveScene(context);
    if (!scene)
        return;

    Actor* actor = ResolveActor(*scene);
    if (!actor)
        return;

    actor->SetName(m_BeforeName);
}

inline SetPropertyCommand::SetPropertyCommand(
    void* object,
    const Rebel::Core::Reflection::PropertyInfo& property,
    const Rebel::Core::Reflection::TypeInfo* ownerType,
    PropertyValue before,
    PropertyValue after)
    : m_Object(object)
    , m_Property(property)
    , m_OwnerType(ownerType)
    , m_Before(std::move(before))
    , m_After(std::move(after))
{
}

inline int32 ReadEnumPropertyValueAsInt32(const void* fieldPtr, const MemSize size)
{
    if (!fieldPtr)
        return 0;

    switch (size)
    {
    case sizeof(uint8):
        return static_cast<int32>(*reinterpret_cast<const uint8*>(fieldPtr));
    case sizeof(uint16):
        return static_cast<int32>(*reinterpret_cast<const uint16*>(fieldPtr));
    case sizeof(uint32):
        return static_cast<int32>(*reinterpret_cast<const uint32*>(fieldPtr));
    case sizeof(uint64):
        return static_cast<int32>(*reinterpret_cast<const uint64*>(fieldPtr));
    default:
        return 0;
    }
}

inline bool WriteEnumPropertyValueFromInt32(void* fieldPtr, const MemSize size, const int32 value)
{
    if (!fieldPtr)
        return false;

    switch (size)
    {
    case sizeof(uint8):
        *reinterpret_cast<uint8*>(fieldPtr) = static_cast<uint8>(value);
        return true;
    case sizeof(uint16):
        *reinterpret_cast<uint16*>(fieldPtr) = static_cast<uint16>(value);
        return true;
    case sizeof(uint32):
        *reinterpret_cast<uint32*>(fieldPtr) = static_cast<uint32>(value);
        return true;
    case sizeof(uint64):
        *reinterpret_cast<uint64*>(fieldPtr) = static_cast<uint64>(value);
        return true;
    default:
        return false;
    }
}

inline SetPropertyCommand::PropertyValue SetPropertyCommand::Capture(
    void* object,
    const Rebel::Core::Reflection::PropertyInfo& property)
{
    using namespace Rebel::Core::Reflection;

    if (!object)
        return std::monostate{};

    void* fieldPtr = GetPropertyPointer(object, property);
    if (!fieldPtr)
        return std::monostate{};

    switch (property.Type)
    {
    case EPropertyType::Int32:
        return *reinterpret_cast<int32*>(fieldPtr);
    case EPropertyType::Enum:
        return ReadEnumPropertyValueAsInt32(fieldPtr, property.Size);
    case EPropertyType::UInt64:
        return *reinterpret_cast<uint64*>(fieldPtr);
    case EPropertyType::Float:
        return *reinterpret_cast<float*>(fieldPtr);
    case EPropertyType::Bool:
        return *reinterpret_cast<bool*>(fieldPtr);
    case EPropertyType::String:
        return *reinterpret_cast<String*>(fieldPtr);
    case EPropertyType::Vector3:
        return *reinterpret_cast<Vector3*>(fieldPtr);
    case EPropertyType::Asset:
    {
        auto* assetPtr = static_cast<AssetPtrBase*>(fieldPtr);
        return assetPtr ? assetPtr->GetHandle() : AssetHandle(0);
    }
    case EPropertyType::MaterialHandle:
        return *reinterpret_cast<MaterialHandle*>(fieldPtr);
    case EPropertyType::Class:
    {
        const Rebel::Core::Reflection::TypeInfo* type = nullptr;
        memcpy(&type, fieldPtr, sizeof(type));
        return type;
    }
    default:
        break;
    }

    return std::monostate{};
}

inline bool SetPropertyCommand::IsSceneRotationProperty() const
{
    if (!m_OwnerType)
        return false;

    if (!m_OwnerType->IsA(SceneComponent::StaticType()))
        return false;

    if (m_Property.Type != Rebel::Core::Reflection::EPropertyType::Vector3)
        return false;

    const char* propertyName = m_Property.Name.c_str();
    if (!propertyName)
        return false;

    return std::strstr(propertyName, "Rotation") != nullptr ||
           std::strstr(propertyName, "rotation") != nullptr;
}

inline bool SetPropertyCommand::Apply(const PropertyValue& value) const
{
    using namespace Rebel::Core::Reflection;

    if (!m_Object)
        return false;

    void* fieldPtr = GetPropertyPointer(m_Object, m_Property);
    if (!fieldPtr)
        return false;

    switch (m_Property.Type)
    {
    case EPropertyType::Int32:
    {
        const int32* v = std::get_if<int32>(&value);
        if (!v)
            return false;

        *reinterpret_cast<int32*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::Enum:
    {
        const int32* v = std::get_if<int32>(&value);
        if (!v)
            return false;

        return WriteEnumPropertyValueFromInt32(fieldPtr, m_Property.Size, *v);
    }
    case EPropertyType::UInt64:
    {
        const uint64* v = std::get_if<uint64>(&value);
        if (!v)
            return false;

        *reinterpret_cast<uint64*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::Float:
    {
        const float* v = std::get_if<float>(&value);
        if (!v)
            return false;

        *reinterpret_cast<float*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::Bool:
    {
        const bool* v = std::get_if<bool>(&value);
        if (!v)
            return false;

        *reinterpret_cast<bool*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::String:
    {
        const String* v = std::get_if<String>(&value);
        if (!v)
            return false;

        *reinterpret_cast<String*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::Vector3:
    {
        const Vector3* v = std::get_if<Vector3>(&value);
        if (!v)
            return false;

        *reinterpret_cast<Vector3*>(fieldPtr) = *v;

        if (IsSceneRotationProperty())
        {
            auto* sceneComponent = static_cast<SceneComponent*>(m_Object);
            sceneComponent->SetRotationEuler(*v);
        }

        return true;
    }
    case EPropertyType::Asset:
    {
        const AssetHandle* v = std::get_if<AssetHandle>(&value);
        if (!v)
            return false;

        auto* assetPtr = static_cast<AssetPtrBase*>(fieldPtr);
        if (!assetPtr)
            return false;

        assetPtr->SetHandle(*v);
        return true;
    }
    case EPropertyType::MaterialHandle:
    {
        const MaterialHandle* v = std::get_if<MaterialHandle>(&value);
        if (!v)
            return false;

        *reinterpret_cast<MaterialHandle*>(fieldPtr) = *v;
        return true;
    }
    case EPropertyType::Class:
    {
        const Rebel::Core::Reflection::TypeInfo* const* v =
            std::get_if<const Rebel::Core::Reflection::TypeInfo*>(&value);
        if (!v)
            return false;

        const Rebel::Core::Reflection::TypeInfo* type = *v;
        if (type && m_Property.SubclassBaseType && !type->IsA(m_Property.SubclassBaseType))
            return false;

        memcpy(fieldPtr, &type, sizeof(type));
        return true;
    }
    default:
        break;
    }

    return false;
}

inline bool SetPropertyCommand::Execute(EditorContext& context)
{
    (void)context;
    return Apply(m_After);
}

inline void SetPropertyCommand::Undo(EditorContext& context)
{
    (void)context;
    Apply(m_Before);
}

inline AssignAssetCommand::AssignAssetCommand(AssetPtrBase* target, AssetHandle before, AssetHandle after)
    : m_Target(target)
    , m_Before(before)
    , m_After(after)
{
}

inline bool AssignAssetCommand::Execute(EditorContext& context)
{
    (void)context;

    if (!m_Target)
        return false;

    m_Target->SetHandle(m_After);
    return true;
}

inline void AssignAssetCommand::Undo(EditorContext& context)
{
    (void)context;

    if (!m_Target)
        return;

    m_Target->SetHandle(m_Before);
}

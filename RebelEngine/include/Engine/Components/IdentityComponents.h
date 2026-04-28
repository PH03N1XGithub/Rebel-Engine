#pragma once

#include <ThirdParty/entt.h>

#include "Core/CoreTypes.h"
#include "Core/GUID.h"
#include "Core/String.h"
#include "Engine/Framework/EngineReflectionExtensions.h"

using namespace Rebel::Core::Reflection;

struct IDComponent
{
    uint64 ID;
    IDComponent()
    {
        ID = (uint64)Rebel::Core::GUID();
    };
    IDComponent(uint64 id) : ID(id) {}
    REFLECTABLE_CLASS(IDComponent, void)
};
REFLECT_CLASS(IDComponent, void)
{
    REFLECT_PROPERTY(IDComponent, ID, EPropertyFlags::VisibleInEditor);
}
END_REFLECT_CLASS(IDComponent)
REFLECT_ECS_COMPONENT(IDComponent)

struct NameComponent
{
    String Name = "Actor";

    NameComponent() = default;
    NameComponent(String& name)
        : Name(name) {}

    REFLECTABLE_CLASS(NameComponent, void)
};

REFLECT_CLASS(NameComponent, void)
{
    REFLECT_PROPERTY(NameComponent, Name,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(NameComponent)
REFLECT_ECS_COMPONENT(NameComponent)

struct ActorTagComponent
{
    String Tag;
    ActorTagComponent() = default;
    ActorTagComponent(String& tag)
        : Tag(tag) {}
    REFLECTABLE_CLASS(ActorTagComponent, void)
};

REFLECT_CLASS(ActorTagComponent, void)
{
    REFLECT_PROPERTY(ActorTagComponent, Tag,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(ActorTagComponent)
REFLECT_ECS_COMPONENT(ActorTagComponent)

struct EntityComponent
{
    virtual ~EntityComponent() = default;
    class Actor* GetOwner() const { return m_Owner; }
    void SetOwner(class Actor* owner) { m_Owner = owner; }
    const String& GetEditorName() const { return m_EditorName; }
    void SetEditorName(const String& name) { m_EditorName = name; }

    entt::entity GetECSHandle() const { return m_ECSHandle; }
    void SetECSHandle(entt::entity handle) { m_ECSHandle = handle; }

    Bool HasBegunPlay() const { return m_bHasBegunPlay; }
    void SetHasBegunPlay(Bool hasBegunPlay) { m_bHasBegunPlay = hasBegunPlay; }

    REFLECTABLE_CLASS(EntityComponent, void)

private:
    class Actor* m_Owner = nullptr;
    entt::entity m_ECSHandle = entt::null;
    Bool m_bHasBegunPlay = false;
    String m_EditorName;
};
REFLECT_CLASS(EntityComponent, void)
{
    REFLECT_PROPERTY(EntityComponent, m_EditorName,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(EntityComponent)

struct TagComponent : EntityComponent
{
    ~TagComponent() = default;

public:
    String ComponentTag;

    TagComponent() = default;
    TagComponent(String& tag)
        : ComponentTag(tag) {}

    REFLECTABLE_CLASS(TagComponent, EntityComponent)
};

REFLECT_CLASS(TagComponent, EntityComponent)
{
    REFLECT_PROPERTY(TagComponent, ComponentTag,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(TagComponent)
REFLECT_ECS_COMPONENT(TagComponent)

struct ActorComponent : TagComponent
{
    virtual ~ActorComponent() = default;
    virtual void OnCreate() {}
    virtual void BeginPlay() {}
    virtual void Tick(float deltaTime) {}

    REFLECTABLE_CLASS(ActorComponent, TagComponent)
};
REFLECT_ABSTRACT_CLASS(ActorComponent, TagComponent)

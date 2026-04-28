#include "Engine/Framework/EnginePch.h"
#include "Engine/Scene/ActorTemplateSerializer.h"

#include "Engine/Components/SceneComponent.h"
#include "Engine/Components/IdentityComponents.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Scene/Scene.h"

namespace
{
bool ShouldSerializeComponent(
    const Rebel::Core::Reflection::ComponentTypeInfo& componentInfo,
    const ActorTemplateSerializer::SerializeOptions& options)
{
    if (!componentInfo.Type)
        return false;

    if (!options.bSerializeIdentityComponents &&
        componentInfo.Type->IsA(IDComponent::StaticType()))
    {
        return false;
    }

    return true;
}

bool IsObjectComponentType(const Rebel::Core::Reflection::ComponentTypeInfo& componentInfo)
{
    return componentInfo.Type && componentInfo.Type->IsA(EntityComponent::StaticType());
}

const Rebel::Core::Reflection::ComponentTypeInfo* FindComponentInfoForType(
    const Rebel::Core::Reflection::TypeInfo* type)
{
    if (!type)
        return nullptr;

    for (const auto& componentInfo : Rebel::Core::Reflection::ComponentRegistry::Get().GetComponents())
    {
        if (componentInfo.Type == type)
            return &componentInfo;
    }

    return nullptr;
}

EntityComponent* FindNewObjectComponent(
    Actor& actor,
    const Rebel::Core::Reflection::TypeInfo* type,
    const TArray<EntityComponent*>& beforeComponents)
{
    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component || component->GetType() != type)
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
            return component;
    }

    return nullptr;
}

EntityComponent* AddObjectComponentInstance(
    Actor& actor,
    const Rebel::Core::Reflection::ComponentTypeInfo& componentInfo)
{
    if (!componentInfo.AddFn || !IsObjectComponentType(componentInfo))
        return nullptr;

    TArray<EntityComponent*> beforeComponents;
    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        if (EntityComponent* component = componentPtr.Get())
            beforeComponents.Add(component);
    }

    componentInfo.AddFn(actor);
    return FindNewObjectComponent(actor, componentInfo.Type, beforeComponents);
}

EntityComponent* FindReusableDefaultObjectComponent(
    Actor& actor,
    const Rebel::Core::Reflection::ComponentTypeInfo& componentInfo)
{
    if (!componentInfo.Type)
        return nullptr;

    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component || component->GetType() != componentInfo.Type)
            continue;

        const String defaultName = componentInfo.Name;
        if (component->GetEditorName() == defaultName)
            return component;
    }

    return nullptr;
}

void NormalizeSceneComponentAfterDeserialize(const Rebel::Core::Reflection::ComponentTypeInfo& componentInfo, void* componentPtr)
{
    if (componentInfo.Type && componentInfo.Type->IsA(SceneComponent::StaticType()) && componentPtr)
    {
        auto* sceneComponent = reinterpret_cast<SceneComponent*>(componentPtr);
        sceneComponent->SetRotationEuler(sceneComponent->GetRotationEuler());
    }
}
}

void ActorTemplateSerializer::SerializeActorTemplate(
    Rebel::Core::Serialization::YamlSerializer& serializer,
    Actor& actor,
    const SerializeOptions& options)
{
    serializer.SerializeType(actor.GetType(), &actor);

    serializer.BeginObject("Components");

    serializer.BeginArray("ObjectComponents");
    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component)
            continue;

        const auto* componentInfo = FindComponentInfoForType(component->GetType());
        if (!componentInfo || !ShouldSerializeComponent(*componentInfo, options))
            continue;

        serializer.BeginArrayElement();
        serializer.Write("Type", componentInfo->Name);
        serializer.Write("Name", component->GetEditorName());
        serializer.BeginObject("Properties");
        serializer.SerializeTypeRecursive(componentInfo->Type, component);
        serializer.EndObject();
        serializer.EndArrayElement();
    }
    serializer.PopNode();

    for (const auto& componentInfo : Rebel::Core::Reflection::ComponentRegistry::Get().GetComponents())
    {
        if (!componentInfo.HasFn || !componentInfo.GetFn)
            continue;

        if (IsObjectComponentType(componentInfo))
            continue;

        if (!componentInfo.HasFn(actor) || !ShouldSerializeComponent(componentInfo, options))
            continue;

        void* componentPtr = componentInfo.GetFn(actor);
        if (!componentPtr)
            continue;

        serializer.SerializeType(componentInfo.Type, componentPtr);
    }
    serializer.EndObject();
}

Actor* ActorTemplateSerializer::DeserializeActorTemplate(
    Scene& scene,
    Rebel::Core::Serialization::YamlSerializer& serializer,
    const YAML::Node& actorNode)
{
    if (!actorNode || !actorNode.IsMap())
        return nullptr;

    String actorTypeName = "Actor";
    YAML::Node actorDataNode;

    for (auto it = actorNode.begin(); it != actorNode.end(); ++it)
    {
        const String key = it->first.as<String>();
        if (key == "Components")
            continue;

        actorTypeName = key;
        actorDataNode = it->second;
        break;
    }

    const auto* actorType = Rebel::Core::Reflection::TypeRegistry::Get().GetType(actorTypeName);
    if (!actorType || !actorType->IsA(Actor::StaticType()))
        actorType = Actor::StaticType();

    Actor& actor = scene.SpawnActor(actorType, true);

    if (actorDataNode && actorDataNode.IsMap())
    {
        serializer.PushNode(actorDataNode);
        serializer.DeserializeTypeRecursive(actorType, &actor);
        serializer.PopNode();
    }

    YAML::Node componentsNode = actorNode["Components"];
    if (componentsNode && componentsNode.IsMap())
    {
        YAML::Node objectComponentsNode = componentsNode["ObjectComponents"];
        if (objectComponentsNode && objectComponentsNode.IsSequence())
        {
            for (const YAML::Node& componentNode : objectComponentsNode)
            {
                if (!componentNode || !componentNode.IsMap())
                    continue;

                const YAML::Node typeNode = componentNode["Type"];
                if (!typeNode || !typeNode.IsScalar())
                    continue;

                const String componentName = typeNode.as<String>();
                const auto* componentInfo =
                    Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(componentName);
                if (!componentInfo || !componentInfo->Type || !IsObjectComponentType(*componentInfo))
                    continue;

                EntityComponent* component = FindReusableDefaultObjectComponent(actor, *componentInfo);
                if (!component)
                    component = AddObjectComponentInstance(actor, *componentInfo);
                if (!component)
                    continue;

                YAML::Node nameNode = componentNode["Name"];
                if (nameNode && nameNode.IsScalar())
                    component->SetEditorName(nameNode.as<String>());

                YAML::Node propertiesNode = componentNode["Properties"];
                if (propertiesNode && propertiesNode.IsMap())
                {
                    serializer.PushNode(propertiesNode);
                    serializer.DeserializeTypeRecursive(componentInfo->Type, component);
                    serializer.PopNode();
                    NormalizeSceneComponentAfterDeserialize(*componentInfo, component);
                }
            }
        }

        for (auto it = componentsNode.begin(); it != componentsNode.end(); ++it)
        {
            const String componentName = it->first.as<String>();
            if (componentName == "ObjectComponents")
                continue;

            const YAML::Node componentDataNode = it->second;

            const auto* componentInfo =
                Rebel::Core::Reflection::ComponentRegistry::Get().FindByName(componentName);
            if (!componentInfo || !componentInfo->Type || !componentInfo->AddFn || !componentInfo->GetFn)
                continue;

            void* componentPtr = nullptr;
            if (IsObjectComponentType(*componentInfo))
            {
                componentPtr = FindReusableDefaultObjectComponent(actor, *componentInfo);
                if (!componentPtr)
                    componentPtr = AddObjectComponentInstance(actor, *componentInfo);
            }
            else
            {
                if (!componentInfo->HasFn || !componentInfo->HasFn(actor))
                    componentInfo->AddFn(actor);
                componentPtr = componentInfo->GetFn(actor);
            }

            if (!componentPtr)
                continue;

            if (!componentDataNode || !componentDataNode.IsMap())
                continue;

            serializer.PushNode(componentDataNode);
            serializer.DeserializeTypeRecursive(componentInfo->Type, componentPtr);
            serializer.PopNode();

            NormalizeSceneComponentAfterDeserialize(*componentInfo, componentPtr);
        }
    }

    scene.FinalizeDeferredActorSpawn(actor);

    return &actor;
}

bool ActorTemplateSerializer::SerializeActorTemplateToString(
    Actor& actor,
    String& outYaml,
    const SerializeOptions& options)
{
    Rebel::Core::Serialization::YamlSerializer serializer;
    serializer.Reset();
    serializer.BeginObject("Prefab");
    serializer.BeginObject("ActorTemplate");
    SerializeActorTemplate(serializer, actor, options);
    serializer.EndObject();
    serializer.EndObject();
    outYaml = serializer.ToString();
    return outYaml.length() > 0;
}

Actor* ActorTemplateSerializer::SpawnActorTemplateFromString(Scene& scene, const String& yamlText)
{
    Rebel::Core::Serialization::YamlSerializer serializer;
    if (!serializer.LoadFromString(yamlText))
        return nullptr;

    serializer.BeginObjectRead("Prefab");
    const YAML::Node prefabNode = serializer.Current();
    if (!prefabNode || !prefabNode.IsMap())
    {
        serializer.EndObjectRead();
        return nullptr;
    }

    const YAML::Node actorNode = prefabNode["ActorTemplate"];
    Actor* actor = DeserializeActorTemplate(scene, serializer, actorNode);
    serializer.EndObjectRead();
    return actor;
}

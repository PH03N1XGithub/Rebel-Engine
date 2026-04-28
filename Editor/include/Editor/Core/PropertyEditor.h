#pragma once

#include "Engine/Framework/EnginePch.h"

class Actor;
struct EntityComponent;

namespace Rebel::Core::Reflection
{
    struct PropertyInfo;
    struct TypeInfo;
}

class PropertyEditor
{
public:
    static void DrawPropertyUI(
        void* object,
        const Rebel::Core::Reflection::PropertyInfo& prop,
        const Rebel::Core::Reflection::TypeInfo* ownerType);
    static void DrawReflectedObjectUI(void* object, const Rebel::Core::Reflection::TypeInfo& type);

    void DrawComponentsForActor(
        Actor& actor,
        const Rebel::Core::Reflection::TypeInfo* selectedComponentType,
        EntityComponent* selectedComponent = nullptr) const;
};

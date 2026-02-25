// EngineReflectionExtensions.h
#pragma once

#include "Core/Reflection.h"
#include "Core/Math/CoreMath.h"  // Vector3, Mat4, etc.
#include "Camera.h"
#include "Mesh.h"
#include "AssetManager/AssetPtr.h"
#include "ThirdParty/entt.h"
#include "Actor.h"

namespace Rebel::Core::Reflection
{
    struct ComponentTypeInfo
    {
        String Name;                              // "SceneComponent"
        const TypeInfo* Type = nullptr;           // pointer to reflection TypeInfo

        // ECS hook functions
        void (*AddFn)(Actor&) = nullptr;
        bool (*HasFn)(Actor&) = nullptr;
        void* (*GetFn)(Actor&) = nullptr;
    };

    class ComponentRegistry
    {
    public:
        static ComponentRegistry& Get()
        {
            static ComponentRegistry instance;
            return instance;
        }

        void RegisterComponent(const ComponentTypeInfo& info)
        {
            for (const auto& existing : m_Components)
            {
                if (existing.Type == info.Type || existing.Name == info.Name)
                    return; // already registered
            }
            
            m_Components.Add(info);
        }

        const Memory::TArray<ComponentTypeInfo>& GetComponents() const
        {
            return m_Components;
        }

        const ComponentTypeInfo* FindByName(const String& name) const
        {
            for (const auto& c : m_Components)
            {
                if (c.Name == name)
                    return &c;
            }
            return nullptr;
        }

    private:
        Memory::TArray<ComponentTypeInfo> m_Components;
    };

    // Optional helper
    template<typename T>
    inline const TypeInfo* GetTypeInfo()
    {
        return T::StaticType();
    }

    
    // Example: upgrade Vector3
    enum class EPropertyType : uint8; // forward if needed

    // Specialize PropertyTypeDeduce for engine math types
    template<>
    struct PropertyTypeDeduce<Vector3>
    {
        // You can use Float or add Vec3 to EPropertyType
        static constexpr EPropertyType value = EPropertyType::Vector3;
    };
    /*template<>
    struct PropertyTypeDeduce<MeshHandle>
    {
        // You can use Float or add Vec3 to EPropertyType
        static constexpr EPropertyType value = EPropertyType::MeshHandle;
    };*/

    template<>
   struct PropertyTypeDeduce<MaterialHandle>
    {
        // You can use Float or add Vec3 to EPropertyType
        static constexpr EPropertyType value = EPropertyType::MaterialHandle;
    };

    template<typename T>
    struct PropertyTypeDeduce<AssetPtr<T>>
    {
        static constexpr EPropertyType value = EPropertyType::Asset;
    };


    
    // --- Helper to detect reflectable types (have StaticType) ---

    template<typename T, typename = void>
    struct HasStaticType : std::false_type {};

    template<typename T>
    struct HasStaticType<T, std::void_t<decltype(T::StaticType())>> : std::true_type {};

    template<typename T>
    constexpr const TypeInfo* TryGetClassType()
    {
        if constexpr (HasStaticType<T>::value)
            return T::StaticType();
        else
            return nullptr;
    }

    // --- Engine-side macro for object properties (nested reflection) ---

    // Use this when a field is a reflectable OBJECT (e.g. Camera inside CameraComponent)
    #define REFLECT_OBJECT_PROPERTY(TYPE, FIELD, FLAGS)                                      \
        info.Properties.Add({                                                               \
            #FIELD,                                                                         \
            offsetof(TYPE, FIELD),                                                          \
            sizeof(((TYPE*)0)->FIELD),                                                      \
            FLAGS,                                                                          \
            Rebel::Core::Reflection::PropertyTypeDeduce<                                   \
                decltype(((TYPE*)0)->FIELD)                                                \
            >::value,                                                                       \
            Rebel::Core::Reflection::TryGetClassType<                                      \
                decltype(((TYPE*)0)->FIELD)                                                \
            >()                                                                             \
        })


    #define REFLECT_ECS_COMPONENT(TYPE)                                        \
    namespace {                                                                \
        struct TYPE##__ComponentRegistration                                   \
        {                                                                      \
            TYPE##__ComponentRegistration()                                    \
            {                                                                  \
                using namespace Rebel::Core::Reflection;                       \
                ComponentTypeInfo info;                                        \
                info.Name = #TYPE;                                             \
                info.Type = TYPE::StaticType();                                \
                                                                               \
                info.AddFn = [](Actor& actor)           \
                {                                                              \
                    actor.AddComponent<TYPE>();                                     \
                };                                                             \
                                                                               \
                info.HasFn = [](Actor& actor) -> bool   \
                {                                                              \
                    return actor.HasComponent<TYPE>();                                \
                };                                                             \
                                                                               \
                info.GetFn = [](Actor& actor) -> void*  \
                {                                                              \
                    return &actor.GetComponent<TYPE>();                                  \
                };                                                             \
                                                                               \
                ComponentRegistry::Get().RegisterComponent(info);              \
            }                                                                  \
        };                                                                     \
        static TYPE##__ComponentRegistration s_##TYPE##__ComponentRegistration;\
    }



    
}

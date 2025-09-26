#pragma once
#include <string>
#include <iostream>
#include "Containers/TArray.h"
#include "Containers/TMap.h"

namespace Rebel::Core::Reflection
{
    // =============================================================
    // Flags for editor/serialization
    // =============================================================
    enum class EPropertyFlags : uint32
    {
        None            = 0,
        VisibleInEditor = BIT(0), 
        SaveGame        = BIT(1),
        Transient       = BIT(2),
        Editable        = BIT(3)
    };

    inline EPropertyFlags operator|(EPropertyFlags a, EPropertyFlags b)
    {
        return static_cast<EPropertyFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
    }

    inline bool HasFlag(EPropertyFlags value, EPropertyFlags flag)
    {
        return (static_cast<uint32>(value) & static_cast<uint32>(flag)) != 0;
    }

    // =============================================================
    // Property type info
    // =============================================================
    enum class EPropertyType : uint8
    {
        Int,
        Float,
        Bool,
        String,
        Unknown
    };

    struct TypeInfo; // Forward declaration for object references

    // =============================================================
    // Deduce EPropertyType from C++ type automatically
    // =============================================================
    template<typename T>
    struct PropertyTypeDeduce
    {
        static constexpr EPropertyType value = EPropertyType::Unknown;
    };

    template<> struct PropertyTypeDeduce<int>    { static constexpr EPropertyType value = EPropertyType::Int; };
    template<> struct PropertyTypeDeduce<float>  { static constexpr EPropertyType value = EPropertyType::Float; };
    template<> struct PropertyTypeDeduce<bool>   { static constexpr EPropertyType value = EPropertyType::Bool; };
    template<> struct PropertyTypeDeduce<String> { static constexpr EPropertyType value = EPropertyType::String; };

    // Treat all pointer types as Int (your engine uses pointers as 32-bit)
    template<typename T>
    struct PropertyTypeDeduce<T*> { static constexpr EPropertyType value = EPropertyType::Int; };

    // =============================================================
    // Property info
    // =============================================================
    struct PropertyInfo
    {
        String Name;
        MemSize Offset;
        MemSize Size;
        EPropertyFlags Flags = EPropertyFlags::None;
        EPropertyType Type = EPropertyType::Unknown;
        const TypeInfo* ClassType = nullptr; // Optional for object references

        // Generic getter: returns pointer to property data
        template<typename T>
        T* Get(void* obj) const
        {
            return reinterpret_cast<T*>(reinterpret_cast<uint8*>(obj) + Offset);
        }
    };

    // =============================================================
    // Type info for classes/structs
    // =============================================================
    struct TypeInfo
    {
        String Name;
        MemSize Size;
        const TypeInfo* Super = nullptr;  // Base class info
        Memory::TArray<PropertyInfo> Properties;

        // Check if this type is derived from another type
        bool IsA(const TypeInfo* base) const
        {
            const TypeInfo* t = this;
            while (t)
            {
                if (t == base) return true;
                t = t->Super;
            }
            return false;
        }
    };

    // =============================================================
    // Global registry for all reflected types
    // =============================================================
    class TypeRegistry
    {
    public:
        static TypeRegistry& Get()
        {
            static TypeRegistry instance;
            return instance;
        }

        void RegisterType(const TypeInfo& info)
        {
            types[info.Name] = info;
        }

        const TypeInfo* GetType(const String& name)
        {
            TypeInfo* it = types.Find(name);
            if (it != nullptr) 
                return it;
            return nullptr;
        }

    private:
        Memory::TMap<String, TypeInfo> types;
    };

    // =============================================================
    // Helper to get property pointer
    // =============================================================
    inline void* GetPropertyPointer(void* obj, const PropertyInfo& prop)
    {
        return reinterpret_cast<uint8*>(obj) + prop.Offset;
    }
}

// =============================================================
// Reflection macros
// =============================================================
#define REFLECTABLE_CLASS(TYPE, SUPER) \
public: \
    static const Rebel::Core::Reflection::TypeInfo* StaticType() { \
        return Rebel::Core::Reflection::TypeRegistry::Get().GetType(#TYPE); \
    } \
    virtual const Rebel::Core::Reflection::TypeInfo* GetType() const { \
        return TYPE::StaticType(); \
    }

#define REFLECT_CLASS(TYPE, SUPER) \
namespace { \
struct TYPE##_ReflectionHelper { \
    TYPE##_ReflectionHelper() { \
        Rebel::Core::Reflection::TypeInfo info; \
        info.Name = #TYPE; \
        info.Size = sizeof(TYPE); \
        if constexpr (!std::is_same_v<SUPER, void>) { \
            info.Super = Rebel::Core::Reflection::TypeRegistry::Get().GetType(#SUPER); \
        }

#define REFLECT_PROPERTY(TYPE, FIELD, FLAGS) \
info.Properties.Add({ \
    #FIELD, \
    offsetof(TYPE, FIELD), \
    sizeof(((TYPE*)0)->FIELD), \
    FLAGS, \
    Rebel::Core::Reflection::PropertyTypeDeduce<decltype(((TYPE*)0)->FIELD)>::value, \
    nullptr \
})

#define END_REFLECT_CLASS(TYPE) \
        Rebel::Core::Reflection::TypeRegistry::Get().RegisterType(info); \
    } \
}; \
static TYPE##_ReflectionHelper s_##TYPE##_reflectionHelper; \
}

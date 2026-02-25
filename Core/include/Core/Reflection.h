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

    inline uint64 TypeHash(const char* name)
    {
        uint64 hash = 1469598103934665603ull; // FNV-1a 64 bit
        while (*name)
        {
            hash ^= (uint64)(unsigned char)(*name++);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    // =============================================================
    // Property type info
    // =============================================================
    enum class EPropertyType : uint8
    {
        Unknown = 0,

        // ---- Integers ----
        Int8,
        UInt8,
        Int16,
        UInt16,
        Int32,
        UInt32,
        Int64,
        UInt64,

        // ---- Floating point ----
        Float,
        Double,

        // ---- Other ----
        Bool,
        String,
        Enum,

        // ---- Engine types ----
        Vector3,
        Asset,
        MaterialHandle,
    };


    struct TypeInfo; // Forward declaration for object references

    // =============================================================
    // Deduce EPropertyType from C++ type automatically
    // =============================================================
    template<typename T>
    struct PropertyTypeDeduce
    {
        using CleanType = std::remove_cv_t<std::remove_reference_t<T>>;

        static constexpr EPropertyType value =
            std::is_enum_v<CleanType> ? EPropertyType::Enum :
            EPropertyType::Unknown;
    };

    // ---- Boolean ----
    template<> struct PropertyTypeDeduce<Bool>
    {
        static constexpr EPropertyType value = EPropertyType::Bool;
    };

    // ---- Signed integers ----
    template<> struct PropertyTypeDeduce<int8>   { static constexpr EPropertyType value = EPropertyType::Int8; };
    template<> struct PropertyTypeDeduce<int16>  { static constexpr EPropertyType value = EPropertyType::Int16; };
    template<> struct PropertyTypeDeduce<int32>  { static constexpr EPropertyType value = EPropertyType::Int32; };
    template<> struct PropertyTypeDeduce<int64>  { static constexpr EPropertyType value = EPropertyType::Int64; };

    // ---- Unsigned integers ----
    template<> struct PropertyTypeDeduce<uint8>  { static constexpr EPropertyType value = EPropertyType::UInt8; };
    template<> struct PropertyTypeDeduce<uint16> { static constexpr EPropertyType value = EPropertyType::UInt16; };
    template<> struct PropertyTypeDeduce<uint32> { static constexpr EPropertyType value = EPropertyType::UInt32; };
    template<> struct PropertyTypeDeduce<uint64> { static constexpr EPropertyType value = EPropertyType::UInt64; };

    // ---- Floating point ----
    template<> struct PropertyTypeDeduce<Float>  { static constexpr EPropertyType value = EPropertyType::Float; };
    template<> struct PropertyTypeDeduce<Double> { static constexpr EPropertyType value = EPropertyType::Double; };

    // ---- String ----
    template<> struct PropertyTypeDeduce<String>
    {
        static constexpr EPropertyType value = EPropertyType::String;
    };

    struct EnumInfo
    {
        const char* EnumName;
        const char** MemberNames;
        uint32 Count;
    };
    template<typename T>
    inline const EnumInfo& GetEnumInfo()
    {
        static const char* kEmpty[] = { "<unregistered>" };
        static const EnumInfo info  = { "<unregistered>", kEmpty, 1 };
        return info;
    }





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
        const TypeInfo* ClassType = nullptr;
        const EnumInfo* Enum = nullptr;


        template<typename T>
        T* Get(void* obj) const
        {
            return reinterpret_cast<T*>(reinterpret_cast<uint8*>(obj) + Offset);
        }
    };

    using FactoryFn = void* (*)();
    // =============================================================
    // Type info for classes/structs
    // =============================================================
    struct TypeInfo
    {
        String Name;
        MemSize Size;
        const TypeInfo* Super = nullptr;
        Memory::TArray<PropertyInfo> Properties;

        // Optional factory function to construct the type
        FactoryFn CreateInstance = nullptr;

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
            // Avoid duplicates
            if (types.Find(info.Name))
                return;

            // Stable allocation
            TypeInfo* stored = new TypeInfo(info);
            types.Add(stored->Name, stored);   // key is String, value is TypeInfo*
            m_ByHash.Add(TypeHash(stored->Name.c_str()), stored);
        }

        const TypeInfo* GetType(const String& name)
        {
            TypeInfo** it = types.Find(name);
            return it ? *it : nullptr;
        }
        const TypeInfo* GetTypeByHash(uint64 hash)
        {
            return *m_ByHash.Find(hash);
        }

        const Memory::TMap<String, TypeInfo*>& GetTypes() const
        {
            return types;
        }

    private:
        Memory::TMap<String, TypeInfo*> types;   // <--- map<String, TypeInfo*>
        Memory::TMap<uint64, const TypeInfo*> m_ByHash;

    };

    inline void* GetPropertyPointer(void* obj, const PropertyInfo& prop)
    {
        return reinterpret_cast<uint8*>(obj) + prop.Offset;
    }
}

// =============================================================
// Reflection macros (DLL-safe)
// =============================================================
#define REFLECTABLE_CLASS(TYPE, SUPER) \
public: \
    static const Rebel::Core::Reflection::TypeInfo* StaticType() { \
        return Rebel::Core::Reflection::TypeRegistry::Get().GetType(#TYPE); \
    } \
    virtual const Rebel::Core::Reflection::TypeInfo* GetType() const { \
        return TYPE::StaticType(); \
    }

// For abstract/base classes (no factory)
#define REFLECT_ABSTRACT_CLASS(TYPE, SUPER) \
namespace { \
struct TYPE##_ReflectionHelper { \
TYPE##_ReflectionHelper() { \
Rebel::Core::Reflection::TypeInfo info; \
info.Name = #TYPE; \
info.Size = sizeof(TYPE); \
if constexpr (!std::is_same_v<SUPER, void>) { \
info.Super = Rebel::Core::Reflection::TypeRegistry::Get().GetType(#SUPER); \
} \
/* no factory */ \
info.CreateInstance = nullptr; \
Rebel::Core::Reflection::TypeRegistry::Get().RegisterType(info); \
} \
}; \
static TYPE##_ReflectionHelper s_##TYPE##_reflectionHelper; \
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
} \
/* Add factory function for concrete classes */ \
info.CreateInstance = []() -> void* { return new TYPE(); };


#define REFLECT_PROPERTY(TYPE, FIELD, FLAGS)                                      \
do {                                                                              \
using FieldT = std::remove_cv_t<std::remove_reference_t<decltype(((TYPE*)0)->FIELD)>>; \
Rebel::Core::Reflection::PropertyInfo p;                                      \
p.Name   = #FIELD;                                                            \
p.Offset = offsetof(TYPE, FIELD);                                             \
p.Size   = sizeof(((TYPE*)0)->FIELD);                                         \
p.Flags  = FLAGS;                                                             \
p.Type   = Rebel::Core::Reflection::PropertyTypeDeduce<FieldT>::value;        \
p.ClassType = nullptr;                                                        \
p.Enum      = nullptr;                                                        \
\
if constexpr (std::is_enum_v<FieldT>)                                         \
{                                                                             \
p.Enum = &Rebel::Core::Reflection::GetEnumInfo<FieldT>();                 \
}                                                                             \
\
info.Properties.Add(p);                                                       \
} while (0)


#define END_REFLECT_CLASS(TYPE) \
        Rebel::Core::Reflection::TypeRegistry::Get().RegisterType(info); \
    } \
}; \
static TYPE##_ReflectionHelper s_##TYPE##_reflectionHelper; \
}


#define _ENUM_RTTI_INTERNAL_BEGIN(EnumType)                     \
namespace Rebel::Core::Reflection {                          \
inline constexpr const char* EnumType##_EnumName = #EnumType; \
inline const char* EnumType##_MemberNames[] = {

#define _ENUM_RTTI_INTERNAL_END(EnumType)                        \
};                                                       \
template<>                                               \
inline const EnumInfo& GetEnumInfo<EnumType>()           \
{                                                        \
static const EnumInfo info = {                       \
EnumType##_EnumName,                             \
EnumType##_MemberNames,                          \
(uint32)(sizeof(EnumType##_MemberNames) / sizeof(const char*)) \
};                                                    \
return info;                                         \
}                                                        \
}
#define REFLECT_ENUM(EnumType) \
_ENUM_RTTI_INTERNAL_BEGIN(EnumType)

#define ENUM_OPTION(Name) \
#Name,

#define END_ENUM(EnumType) \
_ENUM_RTTI_INTERNAL_END(EnumType)








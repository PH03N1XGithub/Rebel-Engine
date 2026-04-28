#pragma once

#include "Core/Reflection.h"

namespace Rebel
{
    class TSubclassOfBase
    {
    public:
        const Core::Reflection::TypeInfo* Get() const
        {
            return m_Type;
        }

        void SetRaw(const Core::Reflection::TypeInfo* type)
        {
            m_Type = type;
        }

    protected:
        const Core::Reflection::TypeInfo* m_Type = nullptr;
    };

    template<typename T>
    class TSubclassOf : public TSubclassOfBase
    {
    public:
        TSubclassOf() = default;

        TSubclassOf(const Core::Reflection::TypeInfo* type)
        {
            Set(type);
        }

        TSubclassOf& operator=(const Core::Reflection::TypeInfo* type)
        {
            Set(type);
            return *this;
        }

        void Set(const Core::Reflection::TypeInfo* type)
        {
            if (!type)
            {
                m_Type = nullptr;
                return;
            }

            const Core::Reflection::TypeInfo* baseType = T::StaticType();
            if (baseType && !type->IsA(baseType))
                return;

            m_Type = type;
        }

        const Core::Reflection::TypeInfo* GetBaseType() const
        {
            return T::StaticType();
        }
    };
}

namespace Rebel::Core::Reflection
{
    template<typename T>
    struct PropertyTypeDeduce<Rebel::TSubclassOf<T>>
    {
        static constexpr EPropertyType value = EPropertyType::Class;
    };

    template<typename T>
    struct SubclassBaseTypeDeduce<Rebel::TSubclassOf<T>>
    {
        static const TypeInfo* Get()
        {
            return T::StaticType();
        }
    };
}

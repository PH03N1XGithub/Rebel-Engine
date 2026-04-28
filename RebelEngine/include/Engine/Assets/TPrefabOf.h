#pragma once

#include "Engine/Assets/AssetPtr.h"
#include "Engine/Assets/PrefabAsset.h"

namespace Rebel
{
    template<typename TActor>
    class TPrefabOf : public AssetPtr<PrefabAsset>
    {
    public:
        using AssetPtr<PrefabAsset>::AssetPtr;
    };
}

namespace Rebel::Core::Reflection
{
    template<typename TActor>
    struct PropertyTypeDeduce<Rebel::TPrefabOf<TActor>>
    {
        static constexpr EPropertyType value = EPropertyType::Asset;
    };

    template<typename TActor>
    struct SubclassBaseTypeDeduce<Rebel::TPrefabOf<TActor>>
    {
        static const TypeInfo* Get()
        {
            return TActor::StaticType();
        }
    };
}

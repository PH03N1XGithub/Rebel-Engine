#include "catch_amalgamated.hpp"
#include "Core/Containers/TMap.h"

TEST_CASE("TMap heavy tombstone churn preserves lookup correctness", "[core][containers][tmap][tombstone]")
{
    TMap<int32, int32> map;
    constexpr int32 initialCount = 4096;

    for (int32 i = 0; i < initialCount; ++i)
    {
        REQUIRE(map.Add(i, i * 7));
    }

    int32 removedCount = 0;
    for (int32 i = 0; i < initialCount; ++i)
    {
        if ((i % 3) != 0)
        {
            REQUIRE(map.Remove(i));
            ++removedCount;
        }
    }

    REQUIRE(map.Num() == static_cast<Rebel::Core::MemSize>(initialCount - removedCount));

    for (int32 i = 0; i < removedCount; ++i)
    {
        const int32 key = initialCount + i;
        REQUIRE(map.Add(key, key * 13));
    }

    REQUIRE(map.Num() == static_cast<Rebel::Core::MemSize>(initialCount));

    for (int pass = 0; pass < 8; ++pass)
    {
        for (int32 i = 0; i < initialCount; ++i)
        {
            int32* value = map.Find(i);
            if ((i % 3) == 0)
            {
                REQUIRE(value != nullptr);
                REQUIRE(*value == i * 7);
            }
            else
            {
                REQUIRE(value == nullptr);
            }
        }

        for (int32 i = 0; i < removedCount; ++i)
        {
            const int32 key = initialCount + i;
            int32* value = map.Find(key);
            REQUIRE(value != nullptr);
            REQUIRE(*value == key * 13);
        }
    }
}

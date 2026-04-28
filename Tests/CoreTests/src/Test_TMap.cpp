#include "catch_amalgamated.hpp"
#include "Core/Containers/TMap.h"

TEST_CASE("TMap insert/remove stress test", "[core][containers][tmap]")
{
    TMap<int, int> map;
    constexpr int N = 2048;

    for (int i = 0; i < N; ++i)
    {
        const bool inserted = map.Add(i, i * 7);
        REQUIRE(inserted);
    }

    REQUIRE(map.Num() == static_cast<Rebel::Core::MemSize>(N));

    for (int i = 0; i < N; i += 2)
    {
        const bool removed = map.Remove(i);
        REQUIRE(removed);
    }

    for (int i = 1; i < N; i += 2)
    {
        int* value = map.Find(i);
        REQUIRE(value != nullptr);
        REQUIRE(*value == i * 7);
    }
}

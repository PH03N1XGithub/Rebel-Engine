#include "catch_amalgamated.hpp"
#include "Core/Core.h"

TEST_CASE("Reflection null-safe lookup test", "[core][reflection]")
{
    using namespace Rebel::Core::Reflection;

    const uint64 missingHash = 0xDEADBEEFCAFEBABEull;
    const TypeInfo* type = TypeRegistry::Get().GetTypeByHash(missingHash);
    REQUIRE(type == nullptr);
}

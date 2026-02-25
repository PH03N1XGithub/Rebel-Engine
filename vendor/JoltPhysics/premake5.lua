project "JoltPhysics"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

    files
    {
        "Jolt/**.h",
        "Jolt/**.inl",
        "Jolt/**.cpp"
    }

    -- DO NOT compile any samples/tools/tests
    removefiles
    {
        "HelloWorld/**",
        "Samples/**",
        "TestFramework/**",
        "UnitTests/**",
        "JoltViewer/**",
        "PerformanceTest/**",
        "Docs/**",
        "Build/**",
        "Assets/**"
    }

    includedirs
    {
        "." -- so <Jolt/...> resolves
    }

    -- --------------------------
    -- JOLT ABI DEFINES (MUST MATCH RebelEngine EXACTLY)
    -- --------------------------
    defines
    {
        "JPH_PLATFORM_WINDOWS",
        "JPH_COMPILER_MSVC",
        "JPH_ENABLE_ASSERTS=0",
        "JPH_PROFILE_ENABLED=0",
        "JPH_DEBUG_RENDERER=0",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=0",
        "JPH_DOUBLE_PRECISION=0"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions
        {
            "/EHsc",
            "/bigobj",
            "/utf-8"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

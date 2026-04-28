project "RebelEngineTests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)
    debugdir (rootDir .. "/Tests/EngineTests")

    files {
        "src/**.h",
        "src/**.cpp",
        "../../vendor/Catch2/extras/catch_amalgamated.cpp"
    }

    defines {
        "REBELENGINE_DLL",
        "GLFW_INCLUDE_NONE",
        "YAML_CPP_STATIC_DEFINE",
        -- Must match RebelEngine/JoltPhysics ABI flags
        "JPH_PLATFORM_WINDOWS",
        "JPH_COMPILER_MSVC",
        "JPH_ENABLE_ASSERTS=0",
        "JPH_PROFILE_ENABLED=0",
        "JPH_DEBUG_RENDERER=0",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=0",
        "JPH_DOUBLE_PRECISION=0"
    }

    includedirs {
        IncludeDir.Core,
        IncludeDir.RebelEngine,
        IncludeDir.vendor,
        IncludeDir.yaml_cpp,
        IncludeDir.glfw,
        IncludeDir.GLAD,
        IncludeDir.assimp,
        IncludeDir.JoltPhysics,
        "../../vendor/Catch2/extras"
    }

    links {
        "RebelEngine",
        "Core",
        "GLAD",
        "GLFW",
        "opengl32",
        "assimp",
        "yaml-cpp",
        "JoltPhysics"
    }

    filter "system:windows"
        systemversion "latest"
        -- Engine headers assume Core/Engine bootstrap macros from EnginePch.
        buildoptions { "/utf-8", "/FIEngine/Framework/EnginePch.h", "/FS", "/Z7" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

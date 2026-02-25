project "RebelEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)
    debugdir (rootDir .. "/RebelEngine")

    defines {
         "REBELENGINE_DLL" ,
         "GLFW_INCLUDE_NONE",
         "YAML_CPP_STATIC_DEFINE",
         -- --------------------------
        -- JOLT ABI DEFINES (MUST MATCH JoltPhysics EXACTLY)
        -- --------------------------
        "JPH_PLATFORM_WINDOWS",
        "JPH_COMPILER_MSVC",
        "JPH_ENABLE_ASSERTS=0",
        "JPH_PROFILE_ENABLED=0",
        "JPH_DEBUG_RENDERER=0",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=0",
        "JPH_DOUBLE_PRECISION=0"
    }

    pchheader "EnginePch.h"
    pchsource "src/EnginePch.cpp"


    files { "**.h", "**.cpp" }
    includedirs { IncludeDir.RebelEngine, IncludeDir.Core, IncludeDir.vendor, IncludeDir.yaml_cpp, IncludeDir.glfw, IncludeDir.GLAD, IncludeDir.assimp, IncludeDir.JoltPhysics}
    links { "Core","GLAD","GLFW","opengl32","assimp","yaml-cpp","JoltPhysics" }
    dependson { "Core" }


    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

project "Glad"
    kind "StaticLib"
    language "C"
    staticruntime "on"
    
    -- use rootDir defined in the main premake5.lua
    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

    files
    {
        "include/glad/glad.h",
        "include/KHR/khrplatform.h",
        "src/glad.c"
    }

    includedirs
    {
        "include"
    }
    
    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
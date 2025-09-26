project "yaml-cpp"
    kind "StaticLib"
    language "C++"
    staticruntime "on"

    -- use rootDir defined in the main premake5.lua
    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

    files {
        "src/**.h",
        "src/**.cpp",
        "include/**.h"
    }

    includedirs {
        rootDir .. "/vendor/yaml-cpp/include"
    }

    defines {
        "YAML_CPP_STATIC_DEFINE"
    }

    filter "system:windows"
        systemversion "latest"
        cppdialect "C++17"
        staticruntime "on"

    filter "system:linux"
        pic "On"
        systemversion "latest"
        cppdialect "C++17"
        staticruntime "off"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

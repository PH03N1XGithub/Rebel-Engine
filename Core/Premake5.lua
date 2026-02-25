project "Core"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    --location (rootDir .. "/Build")
    location "%{wks.location}"   -- Always "Build"

    targetdir (binDir)
    objdir    (objDir)

     debugdir (rootDir .. "/Core")

    defines { "YAML_CPP_STATIC_DEFINE" }
    defines { "CORE" }

    pchheader "Core/CorePch.h"
    pchsource "src/CorePch.cpp"

    files { "**.h", "**.cpp", "**.natvis" }
    includedirs { IncludeDir.Core, IncludeDir.vendor, IncludeDir.GLM, IncludeDir.spdlog, IncludeDir.yaml_cpp }

    links { "yaml-cpp" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

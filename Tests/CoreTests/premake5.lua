project "RebelCoreTests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)
    debugdir (rootDir .. "/Tests/CoreTests")

    files {
        "src/**.h",
        "src/**.cpp",
        "../../vendor/Catch2/extras/catch_amalgamated.cpp"
    }

    defines {
        "YAML_CPP_STATIC_DEFINE"
    }

    includedirs {
        IncludeDir.Core,
        IncludeDir.vendor,
        IncludeDir.GLM,
        IncludeDir.spdlog,
        IncludeDir.yaml_cpp,
        "../../vendor/Catch2/extras"
    }

    links {
        "Core"
    }

    filter "system:windows"
        systemversion "latest"
        -- Core headers assume pch-provided third-party includes (fmt/yaml).
        buildoptions { "/utf-8", "/FICore/CorePch.h" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"


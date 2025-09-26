project "RebelEngine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

    defines { "REBELENGINE_DLL" }

    files { "**.h", "**.cpp" }
    includedirs { IncludeDir.RebelEngine, IncludeDir.Core, IncludeDir.vendor }
    links { "Core" }
    dependson { "Core" }

    postbuildcommands {
        "if not exist \"%{wks.location}Bin\\%{cfg.buildcfg}\\Editor\" mkdir \"%{wks.location}Bin\\%{cfg.buildcfg}\\Editor\"",
        "if not exist \"%{wks.location}Bin\\%{cfg.buildcfg}\\Game\" mkdir \"%{wks.location}Bin\\%{cfg.buildcfg}\\Game\"",
        "copy /B /Y \"%{cfg.targetdir}\\RebelEngine.dll\" \"%{wks.location}Bin\\%{cfg.buildcfg}\\Editor\\RebelEngine.dll\"",
        "copy /B /Y \"%{cfg.targetdir}\\RebelEngine.dll\" \"%{wks.location}Bin\\%{cfg.buildcfg}\\Game\\RebelEngine.dll\""
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

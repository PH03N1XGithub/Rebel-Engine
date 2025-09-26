project "Game"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

    files { "**.h", "**.cpp" }
    includedirs { IncludeDir.Game, IncludeDir.RebelEngine, IncludeDir.Core, IncludeDir.vendor }
    links { "RebelEngine", "Core" }
    dependson { "RebelEngine", "Core" }

project "Editor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)
    debugdir (rootDir .. "/Editor")

    defines {
        "YAML_CPP_STATIC_DEFINE"
    }

    files { "**.h", "**.cpp" }
    includedirs { IncludeDir.Editor, IncludeDir.RebelEngine, IncludeDir.Core, IncludeDir.vendor,IncludeDir.yaml_cpp, IncludeDir.glfw ,IncludeDir.GLAD,IncludeDir.imgui,IncludeDir.assimp,IncludeDir.ImGuizmo}
    links { "RebelEngine", "Core","ImGui","GLAD","GLFW","assimp","yaml-cpp","ImGuizmo" }
    dependson { "RebelEngine", "Core" }


    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"


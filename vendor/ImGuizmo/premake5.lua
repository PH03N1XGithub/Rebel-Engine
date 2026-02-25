project "ImGuizmo"
	kind "StaticLib"
	language "C++"
    staticruntime "on"

	-- use rootDir defined in the main premake5.lua
    location (rootDir .. "/Build")
    targetdir (binDir)
    objdir    (objDir)

	includedirs
	{
    	rootDir .. "/vendor/imgui"
	}

	links
	{
    	"ImGui"
	}



	files { "ImGuizmo.h", "ImGuizmo.cpp" }

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++17"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

    filter "configurations:Dist"
		runtime "Release"
		optimize "on"
        symbols "off"

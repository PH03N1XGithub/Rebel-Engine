workspace "RebelEngine"
    architecture "x64"
    startproject "Editor"
    configurations { "Debug", "Release" }
    location "Build"

--==============================
-- Root Path (absolute)
--==============================
rootDir = path.getabsolute(".")

--==============================
-- Include Directories
--==============================
IncludeDir = {}
IncludeDir["Core"]        = rootDir .. "/Core/include"
IncludeDir["RebelEngine"] = rootDir .. "/RebelEngine/include"
IncludeDir["Editor"]      = rootDir .. "/Editor/include"
IncludeDir["Game"]        = rootDir .. "/Game/include"
IncludeDir["vendor"]      = rootDir .. "/vendor"
IncludeDir["GLM"]         = rootDir .. "/vendor/glm"
IncludeDir["spdlog"]      = rootDir .. "/vendor/spdlog"
IncludeDir["yaml_cpp"]    = rootDir .. "/vendor/yaml-cpp/include"
IncludeDir["glfw"]    = rootDir .. "/vendor/glfw/include"
IncludeDir["imgui"]    = rootDir .. "/vendor/imgui" 
IncludeDir["GLAD"]    = rootDir .. "/vendor/GLAD/include"
IncludeDir["assimp"]    = rootDir .. "/vendor/assimp/include"
IncludeDir["ImGuizmo"]    = rootDir .. "/vendor/ImGuizmo"
IncludeDir["JoltPhysics"]    = rootDir .. "/vendor/JoltPhysics"

--==============================
-- Output directories
--==============================
outputdir = "%{cfg.buildcfg}/%{prj.name}"
binDir    = rootDir .. "/Build/Bin/" .. outputdir
objDir    = rootDir .. "/Build/Bin-Int/" .. outputdir

--==============================
-- Dependencies
--==============================
group "Dependencies"
    include "vendor/yaml-cpp"
    include "vendor/glfw"
    include "vendor/imgui"
    include "vendor/GLAD"
    include "vendor/assimp"
    include "vendor/ImGuizmo"
    include "vendor/JoltPhysics"
group ""

--==============================
-- Include Projects
--==============================
include "Core/premake5.lua"
include "RebelEngine/premake5.lua"
include "Editor/premake5.lua"
include "Game/premake5.lua"

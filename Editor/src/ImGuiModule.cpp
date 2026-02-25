#include "EditorEngine.h"
#include "ImGUIModule.h"

#include "imgui.h"
#include "ThirdParty/imgui_impl_glfw.h"
#include "ThirdParty/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.h"
#include "imgui_internal.h"
#include "RenderModule.h"
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp> // optional
#include "ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "MeshLoader.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/SkeletalMeshAsset.h"
#include "AssetManager/AssetFileHeader.h"
#include "AssetManager/AssetManagerModule.h"
#include "AssetManager/BaseAsset.h"


//ImGuiModule::ImGuiModule() {}
ImGuiModule::~ImGuiModule() {}

DEFINE_LOG_CATEGORY(EditorGUI)

void ImGuiModule::SetColorTheme()
{
    auto& colors = ImGui::GetStyle().Colors;
    auto& style = ImGui::GetStyle();

    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    
    style.WindowPadding = ImVec2{0,10};

    style.FrameRounding = 2.0f;

    style.TabBorderSize = 1.0f;
    style.TabRounding = 6.0f;

    style.DockingNodeHasCloseButton = false;
    style.DockingSeparatorSize = 1.0f;

    style.WindowMenuButtonPosition = ImGuiDir_Left;//None 

    colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

    // Headers
    colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
    colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
    colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
    colors[ImGuiCol_TabUnfocused] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_Tab];

    // Resize Grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

    // Check Mark
    colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);

    // Slider
    colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);
}

void ImGuiModule::Init()
{
    RB_LOG(ImGuiLog, info, "Init ImGuiModule");

    // Ensure OpenGL context is current before initializing ImGui
    GLFWwindow* glfwWindow = GEngine->GetWindow()->GetGLFWWindow();
    glfwMakeContextCurrent(glfwWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;


    ImGui::StyleColorsDark();
    SetColorTheme();
    
    CHECK_MSG(glfwWindow != nullptr, "GLFW window is null before ImGui init!");
    
    // Initialize backends
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Optional viewport tweaks
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // ImGui window Register
}

void ImGuiModule::BeginFrame()
{
    GLFWwindow* window = static_cast<GLFWwindow*>(GEngine->GetWindow()->GetNativeWindow());
    glfwMakeContextCurrent(window);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // Update display size each frame
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(
        static_cast<float>(GEngine->GetWindow()->GetWidth()),
        static_cast<float>(GEngine->GetWindow()->GetHeight())
    );
}

void ImGuiModule::EndFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Optional: handle multi-viewport windows
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}
static bool g_IsMaximized = false;
static void HandleWindowDragFromMenuBar()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    // Only if mouse is over the menu bar region
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
        return;

    static bool dragging = false;
    static ImVec2 dragStartMouse;
    static int   dragStartX, dragStartY;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        dragging = true;
        dragStartMouse = io.MousePos;
        glfwGetWindowPos(GEngine->GetWindow()->GetGLFWWindow(), &dragStartX, &dragStartY);
    }

    if (dragging)
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            dragging = false;
            return;
        }

        const ImVec2 delta = {io.MousePos.x - dragStartMouse.x , io.MousePos.y - dragStartMouse.y};
        glfwSetWindowPos(GEngine->GetWindow()->GetGLFWWindow(),
            dragStartX + (int)delta.x,
            dragStartY + (int)delta.y);

        glfwRestoreWindow(GEngine->GetWindow()->GetGLFWWindow());
        g_IsMaximized = false;
    }
}


static void DrawWindowButtons()
{
    ImGuiStyle& style = ImGui::GetStyle();

    float buttonHeight = ImGui::GetFrameHeight();
    float buttonWidth  = buttonHeight * 1.4f;

    float fullWidth   = ImGui::GetWindowContentRegionMax().x;
    float buttonsArea = buttonWidth * 3.0f + style.ItemSpacing.x * 2.0f;
    float startX      = fullWidth - buttonsArea;

    ImGui::SameLine(startX);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    //ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.36f, 0.36f, 0.36f, 1.0f));

    // --- Minimize ---
    if (ImGui::Button("-", ImVec2(buttonWidth, buttonHeight)))
        glfwIconifyWindow(GEngine->GetWindow()->GetGLFWWindow());

    ImGui::SameLine();

    // --- Maximize / Restore ---
    const char* maxLabel = "[ ]";


   
    if (ImGui::Button(maxLabel, ImVec2(buttonWidth, buttonHeight)))
    {
        if (!g_IsMaximized)
        {
            glfwMaximizeWindow(GEngine->GetWindow()->GetGLFWWindow());
            g_IsMaximized = true;
        }
        else
        {
            glfwRestoreWindow(GEngine->GetWindow()->GetGLFWWindow());
            g_IsMaximized = false;
        }
    }

    ImGui::SameLine();

    // --- Close ---
    if (ImGui::Button("X", ImVec2(buttonWidth, buttonHeight)))
    {
        auto callback = glfwSetWindowCloseCallback(GEngine->GetWindow()->GetGLFWWindow(), NULL);
        if (callback)
            callback(GEngine->GetWindow()->GetGLFWWindow());
        glfwSetWindowCloseCallback(GEngine->GetWindow()->GetGLFWWindow(), callback);
    }
        

    ImGui::PopStyleVar(2);
    //ImGui::PopStyleColor();
}




float barHeight = 24.9f;
Asset* TestAsset;
MeshAsset* TestMesh;

template<typename T>
bool SaveAssetToFile(const String& filePath, T& asset)
{
    FileStream fs(filePath.c_str(), "wb");
    BinaryWriter ar(fs);

    AssetFileHeader header{};
    header.AssetID  = (uint64)asset.ID;
    header.TypeHash = TypeHash(T::StaticType()->Name.c_str());
    header.Version = 3;

    ar.Write(header);

    header.PayloadOffset = ar.Tell();

    asset.SerializedVersion = header.Version;
    asset.Serialize(ar);

    ar.Seek(0);
    ar.Write(header);

    return true;
}

static bool GShowAnimationImporterWindow = false;
static char GAnimImportSourcePath[512] = "assets/models/Idle.fbx";
static char GAnimImportOutputDirectory[512] = "assets";
static AssetHandle GAnimImportSkeletonHandle = 0;
static String GAnimImportStatus;
static int32 GAnimDebuggerSelectedBone = 0;

static String SanitizeAssetFileName(const String& input)
{
    String result;
    for (size_t i = 0; i < input.length(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (std::isalnum(c) || c == '_')
        {
            const char ch[2] = { static_cast<char>(c), '\0' };
            result.append(ch);
            continue;
        }

        result.append("_");
    }

    if (result.length() == 0)
        result = "Clip";

    return result;
}

static String BuildAnimationAssetPath(
    const String& outputDirectory,
    const String& sourceStem,
    const String& clipName,
    int32 clipIndex)
{
    const String safeSource = SanitizeAssetFileName(sourceStem);
    const String safeClip = SanitizeAssetFileName(clipName);
    const String indexStr = std::to_string(clipIndex).c_str();

    String path = outputDirectory;
    if (path.length() > 0 && path[path.length() - 1] != '/' && path[path.length() - 1] != '\\')
        path += "/";

    path += safeSource;
    path += "_";
    path += safeClip;
    path += "_";
    path += indexStr;
    return path;
}

static float NormalizeDebugAnimationTime(float timeSeconds, float durationSeconds, bool bLooping)
{
    return AnimationRuntime::NormalizePlaybackTime(timeSeconds, durationSeconds, bLooping);
}

template<typename TKey>
static int32 FindDebugKeySpanStart(const TArray<TKey>& keys, float sampleTime)
{
    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 1)
        return 0;

    if (sampleTime <= keys[0].TimeSeconds)
        return 0;

    auto begin = keys.begin();
    auto end = keys.end();
    auto upper = std::lower_bound(
        begin + 1,
        end,
        sampleTime,
        [](const TKey& key, float time)
        {
            return key.TimeSeconds < time;
        });

    if (upper == end)
        return keyCount - 1;

    const int32 upperIndex = static_cast<int32>(upper - begin);
    return upperIndex - 1;
}

static Vector3 SampleDebugAnimationVectorKeys(
    const TArray<AnimationVecKey>& keys,
    float sampleTime,
    int32* outFromKeyIndex = nullptr,
    int32* outToKeyIndex = nullptr,
    float* outAlpha = nullptr)
{
    if (outFromKeyIndex) *outFromKeyIndex = -1;
    if (outToKeyIndex) *outToKeyIndex = -1;
    if (outAlpha) *outAlpha = 0.0f;

    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 0)
        return Vector3(0.0f);

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
    {
        if (outFromKeyIndex) *outFromKeyIndex = 0;
        if (outToKeyIndex) *outToKeyIndex = 0;
        return keys[0].Value;
    }

    const int32 fromIndex = FindDebugKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
    {
        if (outFromKeyIndex) *outFromKeyIndex = keyCount - 1;
        if (outToKeyIndex) *outToKeyIndex = keyCount - 1;
        return keys[keyCount - 1].Value;
    }

    const AnimationVecKey& a = keys[fromIndex];
    const AnimationVecKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
    {
        if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
        if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
        if (outAlpha) *outAlpha = 1.0f;
        return b.Value;
    }

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
    if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
    if (outAlpha) *outAlpha = alpha;
    return FMath::mix(a.Value, b.Value, alpha);
}

static Quaternion SampleDebugAnimationRotationKeys(
    const TArray<AnimationQuatKey>& keys,
    float sampleTime,
    int32* outFromKeyIndex = nullptr,
    int32* outToKeyIndex = nullptr,
    float* outAlpha = nullptr)
{
    if (outFromKeyIndex) *outFromKeyIndex = -1;
    if (outToKeyIndex) *outToKeyIndex = -1;
    if (outAlpha) *outAlpha = 0.0f;

    const int32 keyCount = static_cast<int32>(keys.Num());
    if (keyCount <= 0)
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);

    if (keyCount == 1 || sampleTime <= keys[0].TimeSeconds)
    {
        if (outFromKeyIndex) *outFromKeyIndex = 0;
        if (outToKeyIndex) *outToKeyIndex = 0;
        return FMath::normalize(keys[0].Value);
    }

    const int32 fromIndex = FindDebugKeySpanStart(keys, sampleTime);
    if (fromIndex >= keyCount - 1)
    {
        if (outFromKeyIndex) *outFromKeyIndex = keyCount - 1;
        if (outToKeyIndex) *outToKeyIndex = keyCount - 1;
        return FMath::normalize(keys[keyCount - 1].Value);
    }

    const AnimationQuatKey& a = keys[fromIndex];
    const AnimationQuatKey& b = keys[fromIndex + 1];

    const float range = b.TimeSeconds - a.TimeSeconds;
    if (range <= 1e-6f)
    {
        if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
        if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
        if (outAlpha) *outAlpha = 1.0f;
        return FMath::normalize(b.Value);
    }

    const float alpha = FMath::clamp((sampleTime - a.TimeSeconds) / range, 0.0f, 1.0f);
    Quaternion qb = b.Value;
    if (FMath::dot(a.Value, qb) < 0.0f)
        qb = -qb;

    if (outFromKeyIndex) *outFromKeyIndex = fromIndex;
    if (outToKeyIndex) *outToKeyIndex = fromIndex + 1;
    if (outAlpha) *outAlpha = alpha;

    return FMath::normalize(FMath::slerp(a.Value, qb, alpha));
}

static float QuaternionAngularDifferenceDegrees(const Quaternion& a, const Quaternion& b)
{
    const Quaternion na = FMath::normalize(a);
    const Quaternion nb = FMath::normalize(b);
    const float dotValue = FMath::clamp(std::fabs(FMath::dot(na, nb)), 0.0f, 1.0f);
    const float angleRad = 2.0f * std::acos(dotValue);
    return FMath::degrees(angleRad);
}



void DrawTitleBar()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, barHeight));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBackground|
            ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 5.7f)); // more Y padding

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.f, 1.f, 1.0f));
    
    ImGui::Begin("ThickMenuBar", nullptr,flags);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New");
            ImGui::MenuItem("Open");
            ImGui::MenuItem("Save");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo");
            ImGui::MenuItem("Redo");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("create new asset"))
            {
                TestAsset = GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetManager().Create<Asset>();
            }
            if (ImGui::MenuItem("save asset"))
            {
                GEngine->GetModuleManager().GetModule<AssetManagerModule>()->SaveAssetHeader(*TestAsset);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Binary Assets"))
        {
            if (ImGui::MenuItem("create Static mesh"))
            {
                TArray<Vertex> verts;
                TArray<uint32> indices;

                MeshLoader::LoadMeshFromFile("assets/models/ciri.fbx", verts, indices);

                FileStream fs("assets/ciriStatickMesh.rasset", "wb");
                BinaryWriter ar(fs);

                MeshAsset temp;
                temp.Vertices = verts;
                temp.Indices  = indices;

                //temp.Serialize(ar);

                AssetFileHeader header;
                header.AssetID  = (uint64)temp.ID;                  // or however AssetHandle stores it
                header.TypeHash = TypeHash(MeshAsset::StaticType()->Name.c_str());

                // Reserve space for header
                ar.Write(header);

                // Remember where payload starts
                header.PayloadOffset = ar.Tell();

                // Write the actual mesh data
                temp.Serialize(ar);

                // Seek back and rewrite header with correct offset
                ar.Seek(0);
                ar.Write(header);

            }
            if (ImGui::MenuItem("create skeletal"))
            {
                TArray<Vertex> verts;
                TArray<uint32> indices;
                SkeletonAsset skeleton;

                if (!MeshLoader::LoadSkeletalMeshFromFile(
                    "assets/models/Manny.fbx",
                    verts,
                    indices,
                    skeleton))
                    return;
                
                // --- Skeleton ---
                SaveAssetToFile("assets/Manny_character_skeleton.rasset", skeleton);

                // --- Skeletal Mesh ---
                SkeletalMeshAsset skelMesh;
                skelMesh.Vertices = verts;
                skelMesh.Indices = indices;
                skelMesh.m_Skeleton.SetHandle(skeleton.ID);

                SaveAssetToFile("assets/Manny_character_skelmesh.rasset", skelMesh);
                GEngine->GetModuleManager().GetModule<AssetManagerModule>()->Init();

                std::cout << "Skeletal asset created successfully.\n";
            }
            if (ImGui::MenuItem("import animations"))
            {
                GShowAnimationImporterWindow = true;
            }
            ImGui::EndMenu();
        }
        DrawWindowButtons(); // buttons will be aligned with the menu now
        
        ImGui::EndMenuBar();
    }

    HandleWindowDragFromMenuBar();

    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(3);
}
Actor* GSelectedActor = new Actor();
void DrawLevelEditorToolbar()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.2f, 0.2f, 0.2f, 1.0f});

    ImGui::Begin("Level Toolbar###LevelToolbar", nullptr, flags);

    // ▼ Add Actor menu
    if (ImGui::Button("Place Actors  ▾"))
        ImGui::OpenPopup("PlaceActorsPopup");

    if (ImGui::BeginPopup("PlaceActorsPopup"))
    {
        for (auto types = TypeRegistry::Get().GetTypes(); const auto& type : types)
        {
            if (!type.Value->IsA(Actor::StaticType()))
                continue;
            if (!ImGui::MenuItem(type.Key.c_str()))
                continue;
            GEngine->GetActiveScene()->SpawnActor(type.Value);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        //Rebel::Core::Serialization::YamlSerializer serializer;
        GEngine->GetActiveScene()->Serialize("scene.Ryml");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load scene"))
    {
        GSelectedActor = nullptr; // TODO : move this to level editor(gotta move everythink to level editor :D)
        if (! GEngine->GetActiveScene()->Deserialize("scene.Ryml"))
        {
            RB_LOG(EditorGUI,warn,"Failed to load scene");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Play"))
    {
        GSelectedActor = nullptr;
        if (EditorEngine* editor = static_cast<EditorEngine*>(GEngine))
        {
            RB_LOG(EditorGUI, info, "ActiveScene = {}", (void*)GEngine->GetActiveScene());
            if (editor->IsPlaying())
                editor->StopPlayInEditor();
            else
                editor->StartPlayInEditor();
        }
    }

    if (RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>())
    {
        ImGui::SameLine();
        bool bShowGrid = renderer->IsEditorGridEnabled();
        if (ImGui::Checkbox("Grid", &bShowGrid))
            renderer->SetEditorGridEnabled(bShowGrid);

        ImGui::SameLine();
        if (ImGui::Button("Grid Settings"))
            ImGui::OpenPopup("EditorGridSettingsPopup");

        if (ImGui::BeginPopup("EditorGridSettingsPopup"))
        {
            RenderModule::EditorGridSettings settings = renderer->GetEditorGridSettings();
            bool bChanged = false;

            bChanged |= ImGui::DragFloat("Grid Extent (0 = Infinite)", &settings.GridExtent, 1.0f, 0.0f, 10000.0f, "%.1f");
            bChanged |= ImGui::DragFloat("Cell Spacing", &settings.CellSpacing, 0.05f, 0.05f, 100.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Major Spacing", &settings.MajorLineSpacing, 0.25f, 0.1f, 500.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Fade Distance", &settings.FadeDistance, 1.0f, 8.0f, 10000.0f, "%.1f");
            bChanged |= ImGui::DragFloat("Minor Line Width", &settings.MinorLineWidth, 0.05f, 0.5f, 4.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Major Line Width", &settings.MajorLineWidth, 0.05f, 0.5f, 6.0f, "%.2f");

            bChanged |= ImGui::ColorEdit4("Minor Color", &settings.MinorColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("Major Color", &settings.MajorColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("X Axis Color", &settings.XAxisColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("Y Axis Color", &settings.YAxisColor[0], ImGuiColorEditFlags_AlphaBar);

            if (bChanged)
            {
                settings.CellSpacing = FMath::max(settings.CellSpacing, 0.01f);
                settings.MajorLineSpacing = FMath::max(settings.MajorLineSpacing, settings.CellSpacing);
                settings.FadeDistance = FMath::max(settings.FadeDistance, 1.0f);
                settings.GridExtent = FMath::max(settings.GridExtent, 0.0f);
                settings.MinorLineWidth = FMath::max(settings.MinorLineWidth, 0.5f);
                settings.MajorLineWidth = FMath::max(settings.MajorLineWidth, settings.MinorLineWidth);
                renderer->SetEditorGridSettings(settings);
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Lighting"))
            ImGui::OpenPopup("LightingSettingsPopup");

        if (ImGui::BeginPopup("LightingSettingsPopup"))
        {
            RenderModule::DirectionalLightSettings dirLight = renderer->GetDirectionalLight();
            RenderModule::SkyAmbientSettings skyAmbient = renderer->GetSkyAmbient();
            RenderModule::EnvironmentLightingSettings envLighting = renderer->GetEnvironmentLighting();

            bool dirChanged = false;
            bool skyChanged = false;
            bool envChanged = false;

            ImGui::TextUnformatted("Directional Light");
            dirChanged |= ImGui::DragFloat3("Direction", &dirLight.Direction[0], 0.01f, -1.0f, 1.0f, "%.2f");
            dirChanged |= ImGui::ColorEdit3("Color", &dirLight.Color[0]);
            dirChanged |= ImGui::DragFloat("Intensity", &dirLight.Intensity, 0.01f, 0.0f, 20.0f, "%.2f");
            dirChanged |= ImGui::DragFloat("Specular Intensity", &dirLight.SpecularIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
            dirChanged |= ImGui::DragFloat("Specular Power", &dirLight.SpecularPower, 0.25f, 1.0f, 256.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextUnformatted("Sky Ambient");
            skyChanged |= ImGui::ColorEdit3("Sky Color", &skyAmbient.SkyColor[0]);
            skyChanged |= ImGui::ColorEdit3("Ground Color", &skyAmbient.GroundColor[0]);
            skyChanged |= ImGui::DragFloat("Ambient Intensity", &skyAmbient.Intensity, 0.01f, 0.0f, 5.0f, "%.2f");

            ImGui::Separator();
            ImGui::TextUnformatted("Environment (Future)");
            envChanged |= ImGui::Checkbox("Use Environment Map", &envLighting.UseEnvironmentMap);
            envChanged |= ImGui::DragFloat("Diffuse IBL Intensity", &envLighting.DiffuseIBLIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
            envChanged |= ImGui::DragFloat("Specular IBL Intensity", &envLighting.SpecularIBLIntensity, 0.01f, 0.0f, 8.0f, "%.2f");

            if (dirChanged)
            {
                if (FMath::length(dirLight.Direction) < 1e-4f)
                    dirLight.Direction = Vector3(0.0f, -1.0f, 0.0f);

                dirLight.Intensity = FMath::max(dirLight.Intensity, 0.0f);
                dirLight.SpecularIntensity = FMath::max(dirLight.SpecularIntensity, 0.0f);
                dirLight.SpecularPower = FMath::max(dirLight.SpecularPower, 1.0f);
                renderer->SetDirectionalLight(dirLight);
            }

            if (skyChanged)
            {
                skyAmbient.Intensity = FMath::max(skyAmbient.Intensity, 0.0f);
                renderer->SetSkyAmbient(skyAmbient);
            }

            if (envChanged)
            {
                envLighting.DiffuseIBLIntensity = FMath::max(envLighting.DiffuseIBLIntensity, 0.0f);
                envLighting.SpecularIBLIntensity = FMath::max(envLighting.SpecularIBLIntensity, 0.0f);
                renderer->SetEnvironmentLighting(envLighting);
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}



static void BuildLevelEditorLayout(ImGuiID dockspaceID, const ImVec2& fullSize)
{
    // Clear any previous layout on this dockspace
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, fullSize);

    ImGuiID dock_main   = dockspaceID;
    ImGuiID dock_toolbar;
    ImGuiID dock_left   = 0;
    ImGuiID dock_right  = 0;
    ImGuiID dock_bottom = 0;
    ImGuiID dock_right_bottom = 0;

    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Up, 0.05f, &dock_toolbar, &dock_main);

    // Split main: bottom for Content Browser (25%)
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_main);

    // Split right side from main (Outliner + Details)
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, &dock_right, &dock_main);

    // Split right vertically: Outliner top, Details bottom
    ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.5f, &dock_right_bottom, &dock_right);

    // Now dock specific windows into these nodes
    ImGui::DockBuilderDockWindow("Level Toolbar###LevelToolbar", dock_toolbar);
    // --- Make the toolbar node look like Unreal ---
    ImGuiDockNode* toolbarNode = ImGui::DockBuilderGetNode(dock_toolbar);
    if (toolbarNode)
    {
        toolbarNode->LocalFlags |= 
              ImGuiDockNodeFlags_NoTabBar              // ❌ No tabs
            | ImGuiDockNodeFlags_NoCloseButton         // ❌ No close
            | ImGuiDockNodeFlags_NoDocking             // ❌ Nothing can dock into it
            | ImGuiDockNodeFlags_NoWindowMenuButton    // ❌ No menu
            | ImGuiDockNodeFlags_NoSplit               // ❌ Prevent resizing
            | ImGuiDockNodeFlags_NoResizeX
            | ImGuiDockNodeFlags_NoResizeY
            | ImGuiDockNodeFlags_NoUndocking;               // ❌ Fixed position
    }

    ImGui::DockBuilderDockWindow("Viewport###Viewport",       dock_main);
    ImGui::DockBuilderDockWindow("Outliner###Outliner",       dock_right);
    ImGui::DockBuilderDockWindow("Details###Details",         dock_right_bottom);
    ImGui::DockBuilderDockWindow("Content Browser###Content Browser", dock_bottom);

    ImGui::DockBuilderFinish(dockspaceID);
}


// Selected component type for the Details panel
static const Rebel::Core::Reflection::TypeInfo* GSelectedComponentType = nullptr;


void DrawActorNode(entt::entity entity, entt::registry& registry, Actor& actor)
{
    const auto& sc = actor.GetRootComponent();
    //auto& sc   = registry.get<SceneComponent*>(entity);
    auto& name = registry.get<NameComponent>(entity).Name;



    ImGuiTreeNodeFlags flags =
        (GSelectedActor&&(*GSelectedActor == entity) ? ImGuiTreeNodeFlags_Selected : 0)|
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (!false)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool opened = ImGui::TreeNodeEx(
        (void*)(intptr_t)entity,
        flags,
        name.c_str()
    );

    // TODO: Handle selection here if you want
    
    if (ImGui::IsItemClicked())
        GSelectedActor = GEngine->GetActiveScene()->GetActor(entity);
    
    
    // Right-click context menu (PER ITEM)
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
        {
            GSelectedActor->Destroy();
            GSelectedActor = nullptr;
        }

        ImGui::EndPopup();
    }

}


void DrawMeshHandleField(MeshHandle& mc)
{
    auto* renderModule = GEngine->GetModuleManager().GetModule<RenderModule>();
    auto* ogl          = static_cast<OpenGLRenderAPI*>(renderModule->GetRendererAPI());

    const auto& meshAssets = ogl->m_MeshAssets;
    //const auto& matNames   = renderModule->GetMaterialNames();

    
    // --- Mesh selection combo ---
    int currentMeshIndex = 0;

    // Find which mesh asset matches mc.Mesh (simple memcmp)
    for (int i = 0; i < (int)meshAssets.Num(); ++i)
    {
        if (std::memcmp(&meshAssets[i].Handle, &mc, sizeof(MeshHandle)) == 0)
        {
            currentMeshIndex = i;
            break;
        }
    }

    String meshLabel = meshAssets.IsEmpty()
        ? "None"
        : meshAssets[currentMeshIndex].Name;

    if (ImGui::BeginCombo("Mesh", meshLabel.c_str()))
    {
        for (int i = 0; i < (int)meshAssets.Num(); ++i)
        {
            bool isSelected = (i == currentMeshIndex);
            if (ImGui::Selectable(meshAssets[i].Name.c_str(), isSelected))
            {
                currentMeshIndex = i;
                mc = meshAssets[i].Handle;     // 🔥 this changes the mesh
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    
}

static bool DrawMaterialHandleField(const char* label, MaterialHandle& handle)
{
    bool changed = false;

    // Get renderer + materials
    auto* renderModule = GEngine->GetModuleManager().GetModule<RenderModule>();
    const auto& materials = renderModule->GetMaterials(); // TArray<Material>

    if (materials.IsEmpty())
    {
        ImGui::TextDisabled("%s: <no materials>", label);
        return false;
    }

    int currentIndex = (int)handle.Id;
    if (currentIndex < 0 || currentIndex >= (int)materials.Num())
        currentIndex = 0;

    // Build label like "Material 0"
    char currentLabel[64];
    snprintf(currentLabel, sizeof(currentLabel), "Material %d", currentIndex);

    if (ImGui::BeginCombo(label, currentLabel))
    {
        for (int i = 0; i < (int)materials.Num(); ++i)
        {
            char itemLabel[64];
            snprintf(itemLabel, sizeof(itemLabel), "Material %d", i);

            bool selected = (i == currentIndex);

            if (ImGui::Selectable(itemLabel, selected))
            {
                handle.Id = (uint32)i;
                changed = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return changed;
}

void DrawComponentHierarchy(entt::registry& reg, entt::entity e, Actor& actor)
{
    // Always draw SceneComponent as root
    //auto& sc = reg.get<SceneComponent*>(e);

    ImGuiTreeNodeFlags rootFlags =
          ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Draw root node
    bool rootOpen = ImGui::TreeNodeEx("Scene Root", rootFlags);

    // IMPORTANT: capture click info for this item *right after* TreeNodeEx
    bool rootClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool rootDouble  = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                       ImGui::IsItemHovered();

    // If user single-clicks root → clear component selection
    if (rootClicked && !rootDouble)
    {
        GSelectedComponentType = nullptr;
        // RB_LOG(ImGuiLog, trace, "Clicked Scene Root (clear selection)");
    }

    if (rootOpen)
    {
        for (auto& info : ComponentRegistry::Get().GetComponents())
        {
            if (!info.HasFn || !info.GetFn)  continue;
            if (!info.HasFn(actor))         continue; // entity doesn't have this component
            if (info.Name == "SceneComponent" || info.Name == "IDComponent" || info.Name == "NameComponent")
                continue; // already represented by Scene Root

            ImGuiTreeNodeFlags f =
                  ImGuiTreeNodeFlags_Leaf
                | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::TreeNodeEx(info.Name.c_str(), f);

            // Capture click *for this component* immediately
            bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool dbl     = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                           ImGui::IsItemHovered();

            // Single-click → select component
            if (clicked && !dbl)
            {
                GSelectedComponentType = info.Type;
                // RB_LOG(ImGuiLog, trace, "Selected component: {}", info.Name);
            }

            // No TreePop because of NoTreePushOnOpen
        }

        ImGui::TreePop();
    }
}




// Draw a single property based on EPropertyType and flags
void ImGuiModule::DrawPropertyUI(void* object, const PropertyInfo& prop)
{
    using namespace Rebel::Core::Reflection;
    // If this is a nested reflected object:
    if (prop.ClassType != nullptr)
    {
        void* fieldPtr = GetPropertyPointer(object, prop);

        // Draw as a collapsible group
        if (ImGui::TreeNode(prop.Name.c_str()))
        {
            DrawReflectedObjectUI(fieldPtr, *prop.ClassType);
            ImGui::TreePop();
        }
        return;
    }
    if (!HasFlag(prop.Flags, EPropertyFlags::VisibleInEditor))
        return;

    void* fieldPtr = Rebel::Core::Reflection::GetPropertyPointer(object, prop);

    ImGui::PushID(prop.Name.c_str());

    switch (prop.Type)
    {
    case EPropertyType::Int32:
    {
        int* v = reinterpret_cast<int*>(fieldPtr);
        if (HasFlag(prop.Flags, EPropertyFlags::Editable))
            ImGui::DragInt(prop.Name.c_str(), v, 1.0f);
        else
        {
            ImGui::BeginDisabled();
            ImGui::DragInt(prop.Name.c_str(), v, 1.0f);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::UInt64:
        {
            uint64* v = reinterpret_cast<uint64*>(fieldPtr);
            if (HasFlag(prop.Flags, EPropertyFlags::Editable))
            {
                ImGui::InputScalar(prop.Name.c_str(), ImGuiDataType_U64, v);
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::InputScalar(prop.Name.c_str(), ImGuiDataType_U64, v);
                ImGui::EndDisabled();
            }
            break;
        }
    case EPropertyType::Float:
    {
        float* v = reinterpret_cast<float*>(fieldPtr);
        if (HasFlag(prop.Flags, EPropertyFlags::Editable))
            ImGui::DragFloat(prop.Name.c_str(), v, 0.1f);
        else
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat(prop.Name.c_str(), v, 0.1f);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Bool:
    {
        bool* v = reinterpret_cast<bool*>(fieldPtr);
        if (HasFlag(prop.Flags, EPropertyFlags::Editable))
            ImGui::Checkbox(prop.Name.c_str(), v);
        else
        {
            ImGui::BeginDisabled();
            ImGui::Checkbox(prop.Name.c_str(), v);
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::String:
    {
        String* s = reinterpret_cast<String*>(fieldPtr);

        char buffer[256];
        strncpy_s(buffer, s->c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        if (HasFlag(prop.Flags, EPropertyFlags::Editable))
        {
            if (ImGui::InputText(prop.Name.c_str(), buffer, sizeof(buffer)))
                *s = buffer;
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::InputText(prop.Name.c_str(), buffer, sizeof(buffer));
            ImGui::EndDisabled();
        }
        break;
    }
    case EPropertyType::Vector3:
        {
            Vector3* v = reinterpret_cast<Vector3*>(fieldPtr);

            if (HasFlag(prop.Flags, EPropertyFlags::Editable))
            {
                ImGui::DragFloat3(prop.Name.c_str(), &v->x, 0.1f);

                if (prop.Name == "Rotation")
                {
                    SceneComponent* sc = reinterpret_cast<SceneComponent*>(object);
                    sc->SetRotationEuler(*v); // instead of setting v directly
                }

            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::DragFloat3(prop.Name.c_str(), &v->x, 0.1f);
                ImGui::EndDisabled();
            }
            break;
        }
    case EPropertyType::Asset:
        {
            auto* ptr = static_cast<AssetPtrBase*>(fieldPtr);
            auto type = ptr->GetAssetType();

            auto& reg = GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetRegistry().GetAll();

            // Display current selection
            const char* currentName = "None";

            if ((uint64)ptr->GetHandle() != 0)
            {
                if (auto* meta = reg.Find(ptr->GetHandle()))
                    currentName = meta->Path.c_str();   // or ExtractFilename()
            }

            if (ImGui::BeginCombo(prop.Name.c_str(), currentName))
            {
                // None option
                if (ImGui::Selectable("None", (uint64)ptr->GetHandle() == 0))
                {
                    ptr->SetHandle(0);
                }
                for (auto pair : reg)
                {
                    auto meta = pair.Value;
                    if (type != meta.Type)
                        continue;

                    bool selected = (ptr->GetHandle() == meta.ID);

                    if (ImGui::Selectable(meta.Path.c_str(), selected))
                    {
                        ptr->SetHandle(meta.ID);
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
         
                }
                ImGui::EndCombo();
            }
        }
        break;
    case EPropertyType::MaterialHandle:
        {
            auto* h = reinterpret_cast<MaterialHandle*>(fieldPtr);
            if (HasFlag(prop.Flags, EPropertyFlags::Editable))
                DrawMaterialHandleField(prop.Name.c_str(), *h);
            else
            {
                ImGui::BeginDisabled();
                DrawMaterialHandleField(prop.Name.c_str(), *h);
                ImGui::EndDisabled();
            }
            break;
        }
    case EPropertyType::Unknown:
        ImGui::TextDisabled("%s , %s ( !!unhandled type)", prop.Name.c_str(),prop.Type);
        
        break;
    case EPropertyType::Int8:
        break;
    case EPropertyType::UInt8:
        break;
    case EPropertyType::Int16:
        break;
    case EPropertyType::UInt16:
        break;
    case EPropertyType::UInt32:
        break;
    case EPropertyType::Int64:
        break;
    case EPropertyType::Double:
        break;
    case EPropertyType::Enum:
        {
            if (!prop.Enum)
            {
                ImGui::TextDisabled("%s (enum RTTI missing)", prop.Name.c_str());
                break;
            }

            int* v = reinterpret_cast<int*>(fieldPtr);

            ImGui::Combo(
                prop.Name.c_str(),
                v,
                prop.Enum->MemberNames,
                (int)prop.Enum->Count
            );
            break;
        }
    }

    ImGui::PopID();
}

// Draw all reflected properties of an object
void ImGuiModule::DrawReflectedObjectUI(void* object, const Rebel::Core::Reflection::TypeInfo& type)
{
    if (type.Super)
        DrawReflectedObjectUI(object, *type.Super);
    
    for (const auto& prop : type.Properties)
        DrawPropertyUI(object, prop);
}


void ImGuiModule::DrawComponentsForActor(Actor& actor)
{
    if (!actor || !actor.IsValid())
        return;

    Scene* scene = actor.GetScene();        // or however you access it
    if (!scene)
        return;
    
    auto typeInfo = actor.GetType();
    
    ImGuiTreeNodeFlags flags =
           ImGuiTreeNodeFlags_DefaultOpen |
           ImGuiTreeNodeFlags_Framed |
           ImGuiTreeNodeFlags_SpanAvailWidth |
           ImGuiTreeNodeFlags_AllowItemOverlap |
           ImGuiTreeNodeFlags_FramePadding;

    {
        
        const char* label = typeInfo->Name.c_str();
        ImGui::PushID(label);
        if (ImGui::TreeNodeEx(label, flags))
        {
            DrawReflectedObjectUI(&actor, *typeInfo);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    
    

    for (const auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (!info.HasFn || !info.GetFn || !info.Type)
            continue;

        if (!info.HasFn(actor))
            continue;

        if (info.Name == "NameComponent" /*|| info.Name == "IDComponent"*/)
            continue;

        void* instance = info.GetFn(actor);

        if (GSelectedComponentType != nullptr && info.Type != GSelectedComponentType)
            continue;

        const char* label = info.Name.c_str();

        ImGui::PushID(label);
        bool open = ImGui::TreeNodeEx(label, flags);
        if (open)
        {
            DrawReflectedObjectUI(instance, *info.Type);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

static void AddComponentToEntity(Actor& actor, const ComponentTypeInfo& info)
{
    if (!info.AddFn)
    {
        RB_LOG(ImGuiLog, error, "Component %s has no AddFn!", info.Name.c_str());
        return;
    }

    info.AddFn(actor);   // Your ECS ComponentRegistry add function
}

static glm::mat4 ZUpToYUp()
{
    // rotate -90° around X to turn Z-up into Y-up
    return glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1,0,0));
}

static glm::mat4 YUpToZUp()
{
    // inverse rotation
    return glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));
}
static bool HandleViewportPicking(
    RenderModule* renderer,
    const ImVec2& imagePos,
    const ImVec2& viewportSize,
    entt::entity& outHitEntity)
{
    outHitEntity = entt::null;

    if (!renderer)
        return false;

    // Only pick on left click (avoid glReadPixels stall every frame)
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return false;

    const int32 width  = (int32)viewportSize.x;
    const int32 height = (int32)viewportSize.y;
    if (width <= 0 || height <= 0)
        return false;

    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + viewportSize.x, imagePos.y + viewportSize.y);

    const ImVec2 mouse = ImGui::GetMousePos();

    const bool mouseInImage =
        (mouse.x >= imageMin.x && mouse.x < imageMax.x &&
         mouse.y >= imageMin.y && mouse.y < imageMax.y);

    if (!mouseInImage)
        return false;

    int32 mouseX = (int32)(mouse.x - imageMin.x);
    int32 mouseY = (int32)(mouse.y - imageMin.y);

    // Clamp
    if (mouseX < 0) mouseX = 0;
    if (mouseY < 0) mouseY = 0;
    if (mouseX >= width)  mouseX = width  - 1;
    if (mouseY >= height) mouseY = height - 1;

    const uint32 id = renderer->ReadPickID((uint32)mouseX, (uint32)mouseY);

    if (id == 0)
        return false;

    outHitEntity = (entt::entity)(id - 1);
    return true;
}
bool inside;
static bool bIsGizmoActive = false;
static void DrawViewportGizmo(const ImVec2& viewportPos, const ImVec2& viewportSize)
{
    // Need an actor selected
    Actor* selected = GSelectedActor;
    if (!selected || !selected->IsValid())
        return;

    if (selected->GetHandle() == entt::null)
        return;
auto a = selected->GetHandle();
    // Need a transform source (assuming SceneComponent is your root transform)
    if (!selected->HasComponent<SceneComponent>())
        return;

    auto& sc = selected->GetComponent<SceneComponent>();

    // -------------------------
    // Build matrices
    // -------------------------

    // 1) Camera matrices (YOU must adapt these two lines to your actual camera API)
    // You included "Camera.h" already, so you likely have an editor camera somewhere.
    // Example placeholders:
    float aspect = viewportSize.x / viewportSize.y;
    CameraView cam = GEngine->GetActiveCamera(aspect); // <-- change to your real getter
    //if (!cam) return;
    glm::mat4 view = cam.View;
    glm::mat4 proj = cam.Projection;

    // ImGuizmo expects OpenGL clip by default, but projection conventions can differ.
    // If your camera uses GLM perspective with OpenGL depth range [-1..1], you're fine.
    // If you're using a DX-style projection, you'll need conversion.

    // 2) Object transform matrix
    // If you already store a matrix, use it. Otherwise compose from TRS.
    glm::mat4 parentWorld = glm::mat4(1.0f);

    glm::mat4 model = parentWorld * sc.GetWorldTransform();


    // -------------------------
    // ImGuizmo config
    // -------------------------
    ImGuizmo::SetOrthographic(false);
    //ImGuizmo::SetDrawlist(); // draw to current window draw list
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRectFullScreen();   // disable ImGui clipping
    ImGuizmo::SetDrawlist(dl);


    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    static ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE mode = ImGuizmo::WORLD;

    
    // Snap (optional)
    bool snap = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    float snapVals[3] = { 0.5f, 0.5f, 0.5f };
    if (op == ImGuizmo::ROTATE) snapVals[0] = snapVals[1] = snapVals[2] = 15.0f;

    
    if (inside)
    {
        // Hotkeys (optional)
        if (ImGui::IsKeyPressed(ImGuiKey_W)) op = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) op = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) op = ImGuizmo::SCALE;

    }


    // Manipulate (edits model matrix)
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        op,
        mode,
        glm::value_ptr(model),
        nullptr,
        snap ? snapVals : nullptr
    );
    dl->PopClipRect();


    // -------------------------
    // Apply back to SceneComponent
    // -------------------------
    if (ImGuizmo::IsUsing())
    {
        bIsGizmoActive = true;
        glm::mat4 newWorld = model;

        glm::mat4 invParent = glm::inverse(parentWorld);
        glm::mat4 newLocal  = invParent * newWorld;

        // Decompose LOCAL, not WORLD
        glm::vec3 t, s, skew;
        glm::vec4 perspective;
        glm::quat r;

        glm::decompose(newLocal, s, r, t, skew, perspective);

        sc.Position = { t.x, t.y, t.z };
        sc.Scale    = { s.x, s.y, s.z };
        sc.SetRotationQuat(r);
    }
    else
    {
        bIsGizmoActive = false;
    }
}


bool a = true;

void DrawConsole()
{
    ImGui::Begin("Console");

    // =======================
    // TOOLBAR
    // =======================
    if (ImGui::Button("Clear"))
        GEngineLogBuffer.Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &GConsoleState.AutoScroll);

    ImGui::SameLine();
    ImGui::Checkbox("Show Time", &GConsoleState.ShowTime);

    ImGui::SameLine();

    const char* levels[] = { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("Level", &GConsoleState.MinLogLevel, levels, IM_ARRAYSIZE(levels));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Search", GConsoleState.SearchBuffer, IM_ARRAYSIZE(GConsoleState.SearchBuffer));
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("Categories", "Filter Categories"))
    {
        // Enable All / Disable All
        if (ImGui::Selectable("Enable All"))
        {
            for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
                enabled = true;
        }

        if (ImGui::Selectable("Disable All"))
        {
            for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
                enabled = false;
        }

        ImGui::Separator();

        // Individual category toggles
        for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
        {
            ImGui::Checkbox(cat.c_str(), &enabled);
        }

        ImGui::EndCombo();
    }



    ImGui::Separator();

    // =======================
    // LOG SCROLL AREA
    // =======================
    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& entries = GEngineLogBuffer.Get();

    for (const auto& e : entries)
    {
        // Filter by level
        if ((int)e.Level < GConsoleState.MinLogLevel)
            continue;

        if (!GConsoleState.CategoryFilter[e.Category])
            continue;


        // Filter by search
        if (strlen(GConsoleState.SearchBuffer) > 0)
        {
            if (e.Message.find(GConsoleState.SearchBuffer) == std::string::npos)
                continue;
        }

        // Choose color by level
        ImVec4 color = ImVec4(1, 1, 1, 1);

        switch (e.Level)
        {
            case spdlog::level::trace:    color = ImVec4(0.6f, 0.6f, 0.6f, 1); break;
            case spdlog::level::debug:    color = ImVec4(0.4f, 0.8f, 1.0f, 1); break;
            case spdlog::level::info:     color = ImVec4(1, 1, 1, 1); break;
            case spdlog::level::warn:     color = ImVec4(1.0f, 0.8f, 0.2f, 1); break;
            case spdlog::level::err:      color = ImVec4(1.0f, 0.3f, 0.3f, 1); break;
            case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);

        if (GConsoleState.ShowTime)
        {
            ImGui::TextUnformatted(e.Message.c_str());
        }
        else
        {
            // crude timestamp strip if pattern is [HH:MM:SS]
            const char* msg = e.Message.c_str();
            if (strlen(msg) > 10 && msg[0] == '[')
                msg += 10;

            ImGui::TextUnformatted(msg);
        }

        ImGui::PopStyleColor();
    }

    // Auto-scroll
    if (GConsoleState.AutoScroll &&
        ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}





static void DrawAnimationImporterWindow()
{
    if (!GShowAnimationImporterWindow)
        return;

    ImGui::Begin("Animation Importer", &GShowAnimationImporterWindow);

    ImGui::InputText("Source Animation File", GAnimImportSourcePath, IM_ARRAYSIZE(GAnimImportSourcePath));
    ImGui::InputText("Output Directory", GAnimImportOutputDirectory, IM_ARRAYSIZE(GAnimImportOutputDirectory));

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        ImGui::TextDisabled("Asset manager is unavailable.");
        ImGui::End();
        return;
    }

    const auto& registry = assetModule->GetRegistry().GetAll();

    const char* skeletonLabel = "None";
    if ((uint64)GAnimImportSkeletonHandle != 0)
    {
        if (const AssetMeta* meta = registry.Find(GAnimImportSkeletonHandle))
            skeletonLabel = meta->Path.c_str();
    }

    if (ImGui::BeginCombo("Skeleton", skeletonLabel))
    {
        if (ImGui::Selectable("None", (uint64)GAnimImportSkeletonHandle == 0))
            GAnimImportSkeletonHandle = 0;

        for (const auto& pair : registry)
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != SkeletonAsset::StaticType())
                continue;

            const bool bSelected = (meta.ID == GAnimImportSkeletonHandle);
            if (ImGui::Selectable(meta.Path.c_str(), bSelected))
                GAnimImportSkeletonHandle = meta.ID;

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Import Clips"))
    {
        GAnimImportStatus = "";

        if ((uint64)GAnimImportSkeletonHandle == 0)
        {
            GAnimImportStatus = "Select a skeleton asset before importing.";
        }
        else if (!std::filesystem::exists(std::filesystem::path(GAnimImportSourcePath)))
        {
            GAnimImportStatus = "Source animation file was not found.";
        }
        else
        {
            SkeletonAsset* skeleton =
                dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(GAnimImportSkeletonHandle));

            if (!skeleton)
            {
                GAnimImportStatus = "Failed to load selected skeleton asset.";
            }
            else
            {
                TArray<AnimationAsset> clips;
                if (!MeshLoader::LoadAnimationClipsFromFile(GAnimImportSourcePath, *skeleton, clips))
                {
                    GAnimImportStatus = "No animation clips imported (see log).";
                }
                else
                {
                    const std::filesystem::path sourcePath(GAnimImportSourcePath);
                    const String sourceStem = sourcePath.stem().string().c_str();
                    const String outputDir = String(GAnimImportOutputDirectory);

                    std::filesystem::create_directories(std::filesystem::path(GAnimImportOutputDirectory));

                    int32 savedCount = 0;
                    for (int32 i = 0; i < clips.Num(); ++i)
                    {
                        AnimationAsset& clip = clips[i];
                        clip.Path = BuildAnimationAssetPath(outputDir, sourceStem, clip.m_ClipName, i);

                        const String filePath = clip.Path + ".rasset";
                        if (SaveAssetToFile(filePath, clip))
                            ++savedCount;
                    }

                    assetModule->Init(); // refresh registry so imported clips appear in asset pickers
                    GAnimImportStatus =
                        String("Imported ") +
                        String(std::to_string(savedCount).c_str()) +
                        String(" animation clip(s).");
                }
            }
        }
    }

    if (GAnimImportStatus.length() > 0)
        ImGui::TextWrapped("%s", GAnimImportStatus.c_str());

    ImGui::End();
}

static void DrawAnimationDebuggerWindow()
{
    ImGui::Begin("Animation Debugger");

    Actor* selected = GSelectedActor;
    if (!selected || !selected->IsValid() || !selected->HasComponent<SkeletalMeshComponent>())
    {
        ImGui::TextDisabled("Select an actor with SkeletalMeshComponent.");
        ImGui::End();
        return;
    }

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        ImGui::TextDisabled("Asset manager is unavailable.");
        ImGui::End();
        return;
    }

    SkeletalMeshComponent& skComp = selected->GetComponent<SkeletalMeshComponent>();
    auto& manager = assetModule->GetManager();
    const auto& registry = assetModule->GetRegistry().GetAll();

    SkeletalMeshAsset* skAsset = nullptr;
    SkeletonAsset* skeleton = nullptr;
    if ((uint64)skComp.Mesh.GetHandle() != 0)
    {
        skAsset = dynamic_cast<SkeletalMeshAsset*>(manager.Load(skComp.Mesh.GetHandle()));
        if (skAsset && (uint64)skAsset->m_Skeleton.GetHandle() != 0)
            skeleton = dynamic_cast<SkeletonAsset*>(manager.Load(skAsset->m_Skeleton.GetHandle()));
    }

    const char* animationLabel = "None";
    if ((uint64)skComp.Animation.GetHandle() != 0)
    {
        if (const AssetMeta* meta = registry.Find(skComp.Animation.GetHandle()))
            animationLabel = meta->Path.c_str();
    }

    if (ImGui::BeginCombo("Animation", animationLabel))
    {
        if (ImGui::Selectable("None", (uint64)skComp.Animation.GetHandle() == 0))
        {
            skComp.Animation.SetHandle(0);
            skComp.PlaybackTime = 0.0f;
        }

        for (const auto& pair : registry)
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != AnimationAsset::StaticType())
                continue;

            bool bCompatible = true;
            if (skeleton)
            {
                AnimationAsset* candidate = dynamic_cast<AnimationAsset*>(manager.Load(meta.ID));
                if (candidate && (uint64)candidate->m_SkeletonID != 0 &&
                    (uint64)candidate->m_SkeletonID != (uint64)skeleton->ID)
                {
                    bCompatible = false;
                }
            }

            if (!bCompatible)
                continue;

            const bool bSelected = (skComp.Animation.GetHandle() == meta.ID);
            if (ImGui::Selectable(meta.Path.c_str(), bSelected))
            {
                skComp.Animation.SetHandle(meta.ID);
                skComp.PlaybackTime = 0.0f;
            }

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::Checkbox("Play", &skComp.bPlayAnimation);
    ImGui::Checkbox("Loop", &skComp.bLoopAnimation);
    ImGui::DragFloat("Speed", &skComp.PlaybackSpeed, 0.01f, -4.0f, 4.0f);
    ImGui::DragFloat("Time", &skComp.PlaybackTime, 0.01f, 0.0f, 600.0f);

    AnimationAsset* animation = nullptr;
    if ((uint64)skComp.Animation.GetHandle() != 0)
        animation = dynamic_cast<AnimationAsset*>(manager.Load(skComp.Animation.GetHandle()));
    const float sampleTime = animation
        ? NormalizeDebugAnimationTime(skComp.PlaybackTime, animation->m_DurationSeconds, skComp.bLoopAnimation)
        : 0.0f;

    if (animation)
    {
        ImGui::Text("Clip: %s", animation->m_ClipName.c_str());
        ImGui::Text("Duration: %.3f", animation->m_DurationSeconds);
        ImGui::Text("Tracks: %d", animation->m_Tracks.Num());
        if (animation->m_RootDriver.bEnabled)
        {
            ImGui::Text("Root Driver: %s (T:%d R:%d S:%d)",
                animation->m_RootDriver.NodeName.c_str(),
                animation->m_RootDriver.bAffectsTranslation ? 1 : 0,
                animation->m_RootDriver.bAffectsRotation ? 1 : 0,
                animation->m_RootDriver.bAffectsScale ? 1 : 0);
        }
        else
        {
            ImGui::TextDisabled("Root Driver: none");
        }
    }
    else
    {
        ImGui::TextDisabled("No animation selected.");
    }

    if (!skeleton || skeleton->m_BoneNames.IsEmpty())
    {
        ImGui::TextDisabled("No skeleton data available.");
        ImGui::End();
        return;
    }

    if (GAnimDebuggerSelectedBone < 0 || GAnimDebuggerSelectedBone >= skeleton->m_BoneNames.Num())
        GAnimDebuggerSelectedBone = 0;

    const char* boneLabel = skeleton->m_BoneNames[GAnimDebuggerSelectedBone].c_str();
    if (ImGui::BeginCombo("Bone", boneLabel))
    {
        for (int32 i = 0; i < skeleton->m_BoneNames.Num(); ++i)
        {
            const bool bSelected = (i == GAnimDebuggerSelectedBone);
            if (ImGui::Selectable(skeleton->m_BoneNames[i].c_str(), bSelected))
                GAnimDebuggerSelectedBone = i;

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const int32 boneCount = static_cast<int32>(skeleton->m_BoneNames.Num());
    const bool hasRuntimePoseData =
        skComp.RuntimeBoneLocalTranslations.Num() == boneCount &&
        skComp.RuntimeBoneGlobalTranslations.Num() == boneCount &&
        skComp.RuntimeBoneLocalScales.Num() == boneCount &&
        skComp.RuntimeBoneGlobalScales.Num() == boneCount &&
        skComp.RuntimeBoneLocalRotations.Num() == boneCount &&
        skComp.RuntimeBoneGlobalRotations.Num() == boneCount;

    Vector3 runtimeLocalT(0.0f);
    Vector3 runtimeGlobalT(0.0f);
    Vector3 runtimeLocalS(1.0f);
    Vector3 runtimeGlobalS(1.0f);
    Quaternion runtimeLocalR(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion runtimeGlobalR(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 runtimeLocalEuler(0.0f);
    Vector3 runtimeGlobalEuler(0.0f);
    bool hasAssetPoseData = false;
    Vector3 assetPoseLocalT(0.0f);
    Vector3 assetPoseGlobalT(0.0f);
    Vector3 assetPoseLocalS(1.0f);
    Vector3 assetPoseGlobalS(1.0f);
    Quaternion assetPoseLocalR(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion assetPoseGlobalR(1.0f, 0.0f, 0.0f, 0.0f);
    Vector3 assetPoseLocalEuler(0.0f);
    Vector3 assetPoseGlobalEuler(0.0f);

    if (animation)
    {
        TArray<Mat4> localBindPose;
        TArray<Mat4> globalBindPose;
        if (AnimationRuntime::BuildBindPoses(skeleton, localBindPose, globalBindPose))
        {
            TArray<Mat4> localPose = localBindPose;
            TArray<Mat4> globalPose = globalBindPose;
            AnimationRuntime::EvaluateLocalPose(skeleton, animation, sampleTime, localBindPose, localPose);
            AnimationRuntime::BuildGlobalPose(skeleton, localPose, globalPose);

            if (GAnimDebuggerSelectedBone >= 0 && GAnimDebuggerSelectedBone < localPose.Num())
            {
                AnimationRuntime::DecomposeTRS(localPose[GAnimDebuggerSelectedBone], assetPoseLocalT, assetPoseLocalR, assetPoseLocalS);
                AnimationRuntime::DecomposeTRS(globalPose[GAnimDebuggerSelectedBone], assetPoseGlobalT, assetPoseGlobalR, assetPoseGlobalS);

                assetPoseLocalR = FMath::normalize(assetPoseLocalR);
                assetPoseGlobalR = FMath::normalize(assetPoseGlobalR);
                assetPoseLocalEuler = FMath::degrees(FMath::eulerAngles(assetPoseLocalR));
                assetPoseGlobalEuler = FMath::degrees(FMath::eulerAngles(assetPoseGlobalR));
                hasAssetPoseData = true;
            }
        }
    }

    if (hasRuntimePoseData)
    {
        runtimeLocalT = skComp.RuntimeBoneLocalTranslations[GAnimDebuggerSelectedBone];
        runtimeGlobalT = skComp.RuntimeBoneGlobalTranslations[GAnimDebuggerSelectedBone];
        runtimeLocalS = skComp.RuntimeBoneLocalScales[GAnimDebuggerSelectedBone];
        runtimeGlobalS = skComp.RuntimeBoneGlobalScales[GAnimDebuggerSelectedBone];
        runtimeLocalR = FMath::normalize(skComp.RuntimeBoneLocalRotations[GAnimDebuggerSelectedBone]);
        runtimeGlobalR = FMath::normalize(skComp.RuntimeBoneGlobalRotations[GAnimDebuggerSelectedBone]);
        runtimeLocalEuler = FMath::degrees(FMath::eulerAngles(runtimeLocalR));
        runtimeGlobalEuler = FMath::degrees(FMath::eulerAngles(runtimeGlobalR));

        ImGui::Text("Runtime Local Translation:  %.3f  %.3f  %.3f",
            runtimeLocalT.x, runtimeLocalT.y, runtimeLocalT.z);
        ImGui::Text("Runtime Global Translation: %.3f  %.3f  %.3f",
            runtimeGlobalT.x, runtimeGlobalT.y, runtimeGlobalT.z);

        ImGui::Text("Runtime Local Scale:        %.3f  %.3f  %.3f",
            runtimeLocalS.x, runtimeLocalS.y, runtimeLocalS.z);
        ImGui::Text("Runtime Global Scale:       %.3f  %.3f  %.3f",
            runtimeGlobalS.x, runtimeGlobalS.y, runtimeGlobalS.z);

        ImGui::Text("Runtime Local Rotation Q:  %.3f  %.3f  %.3f  %.3f",
            runtimeLocalR.w, runtimeLocalR.x, runtimeLocalR.y, runtimeLocalR.z);
        ImGui::Text("Runtime Local Rotation E:  %.3f  %.3f  %.3f",
            runtimeLocalEuler.x, runtimeLocalEuler.y, runtimeLocalEuler.z);

        ImGui::Text("Runtime Global Rotation Q: %.3f  %.3f  %.3f  %.3f",
            runtimeGlobalR.w, runtimeGlobalR.x, runtimeGlobalR.y, runtimeGlobalR.z);
        ImGui::Text("Runtime Global Rotation E: %.3f  %.3f  %.3f",
            runtimeGlobalEuler.x, runtimeGlobalEuler.y, runtimeGlobalEuler.z);
    }
    else
    {
        ImGui::TextDisabled("Runtime pose data not available yet.");
    }

    ImGui::Separator();
    ImGui::Text("Asset Pose (Sampled)");

    if (hasAssetPoseData)
    {
        ImGui::Text("Asset Pose Local Translation:  %.3f  %.3f  %.3f",
            assetPoseLocalT.x, assetPoseLocalT.y, assetPoseLocalT.z);
        ImGui::Text("Asset Pose Global Translation: %.3f  %.3f  %.3f",
            assetPoseGlobalT.x, assetPoseGlobalT.y, assetPoseGlobalT.z);

        ImGui::Text("Asset Pose Local Scale:        %.3f  %.3f  %.3f",
            assetPoseLocalS.x, assetPoseLocalS.y, assetPoseLocalS.z);
        ImGui::Text("Asset Pose Global Scale:       %.3f  %.3f  %.3f",
            assetPoseGlobalS.x, assetPoseGlobalS.y, assetPoseGlobalS.z);

        ImGui::Text("Asset Pose Local Rotation Q:  %.3f  %.3f  %.3f  %.3f",
            assetPoseLocalR.w, assetPoseLocalR.x, assetPoseLocalR.y, assetPoseLocalR.z);
        ImGui::Text("Asset Pose Local Rotation E:  %.3f  %.3f  %.3f",
            assetPoseLocalEuler.x, assetPoseLocalEuler.y, assetPoseLocalEuler.z);

        ImGui::Text("Asset Pose Global Rotation Q: %.3f  %.3f  %.3f  %.3f",
            assetPoseGlobalR.w, assetPoseGlobalR.x, assetPoseGlobalR.y, assetPoseGlobalR.z);
        ImGui::Text("Asset Pose Global Rotation E: %.3f  %.3f  %.3f",
            assetPoseGlobalEuler.x, assetPoseGlobalEuler.y, assetPoseGlobalEuler.z);
    }
    else
    {
        ImGui::TextDisabled("Asset pose data not available.");
    }

    if (hasAssetPoseData && hasRuntimePoseData)
    {
        ImGui::Separator();
        ImGui::Text("Asset Pose Delta vs Runtime");

        const Vector3 deltaLocalT = assetPoseLocalT - runtimeLocalT;
        const Vector3 deltaGlobalT = assetPoseGlobalT - runtimeGlobalT;
        const Vector3 deltaLocalS = assetPoseLocalS - runtimeLocalS;
        const Vector3 deltaGlobalS = assetPoseGlobalS - runtimeGlobalS;
        const float deltaLocalQDeg = QuaternionAngularDifferenceDegrees(assetPoseLocalR, runtimeLocalR);
        const float deltaGlobalQDeg = QuaternionAngularDifferenceDegrees(assetPoseGlobalR, runtimeGlobalR);
        const Vector3 deltaLocalEuler = assetPoseLocalEuler - runtimeLocalEuler;
        const Vector3 deltaGlobalEuler = assetPoseGlobalEuler - runtimeGlobalEuler;

        ImGui::Text("Delta Local Translation:  %.3f  %.3f  %.3f (len %.3f)",
            deltaLocalT.x, deltaLocalT.y, deltaLocalT.z, FMath::length(deltaLocalT));
        ImGui::Text("Delta Global Translation: %.3f  %.3f  %.3f (len %.3f)",
            deltaGlobalT.x, deltaGlobalT.y, deltaGlobalT.z, FMath::length(deltaGlobalT));

        ImGui::Text("Delta Local Scale:        %.3f  %.3f  %.3f (len %.3f)",
            deltaLocalS.x, deltaLocalS.y, deltaLocalS.z, FMath::length(deltaLocalS));
        ImGui::Text("Delta Global Scale:       %.3f  %.3f  %.3f (len %.3f)",
            deltaGlobalS.x, deltaGlobalS.y, deltaGlobalS.z, FMath::length(deltaGlobalS));

        ImGui::Text("Delta Local Rotation Q:   %.3f deg", deltaLocalQDeg);
        ImGui::Text("Delta Global Rotation Q:  %.3f deg", deltaGlobalQDeg);
        ImGui::Text("Delta Local Rotation E:   %.3f  %.3f  %.3f",
            deltaLocalEuler.x, deltaLocalEuler.y, deltaLocalEuler.z);
        ImGui::Text("Delta Global Rotation E:  %.3f  %.3f  %.3f",
            deltaGlobalEuler.x, deltaGlobalEuler.y, deltaGlobalEuler.z);
    }

    ImGui::Separator();
    ImGui::Text("Asset Track Data (Raw)");

    if (!animation)
    {
        ImGui::TextDisabled("No animation asset selected.");
    }
    else
    {
        const AnimationTrack* track = animation->FindTrackForBone(GAnimDebuggerSelectedBone);
        if (!track)
        {
            ImGui::TextDisabled("Selected bone has no track in this clip.");
        }
        else
        {
            const int32 positionKeyCount = static_cast<int32>(track->PositionKeys.Num());
            const int32 rotationKeyCount = static_cast<int32>(track->RotationKeys.Num());
            const int32 scaleKeyCount = static_cast<int32>(track->ScaleKeys.Num());

            ImGui::Text("Track Bone: %s", track->BoneName.c_str());
            ImGui::Text("Position Keys: %d", positionKeyCount);
            ImGui::Text("Rotation Keys: %d", rotationKeyCount);
            ImGui::Text("Scale Keys:    %d", scaleKeyCount);
            ImGui::Text("Sample Time: %.3f", sampleTime);

            if (positionKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Vector3 sampled =
                    SampleDebugAnimationVectorKeys(track->PositionKeys, sampleTime, &fromKey, &toKey, &alpha);

                ImGui::Text("Asset Track Translation (Sampled): %.3f  %.3f  %.3f", sampled.x, sampled.y, sampled.z);

                if (hasRuntimePoseData)
                {
                    const Vector3 localTranslationDelta = sampled - runtimeLocalT;
                    const float localTranslationDeltaLen = FMath::length(localTranslationDelta);
                    ImGui::Text("Delta vs Runtime Local T: %.3f  %.3f  %.3f (len %.3f)",
                        localTranslationDelta.x,
                        localTranslationDelta.y,
                        localTranslationDelta.z,
                        localTranslationDeltaLen);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationVecKey& from = track->PositionKeys[fromKey];
                    ImGui::Text("From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationVecKey& to = track->PositionKeys[toKey];
                        ImGui::Text("To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Translation Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        positionKeyCount < maxShownKeys ? positionKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationVecKey& key = track->PositionKeys[i];
                        ImGui::Text(
                            "#%d  t=%.3f  v=(%.3f, %.3f, %.3f)",
                            i, key.TimeSeconds, key.Value.x, key.Value.y, key.Value.z);
                    }

                    if (shownKeyCount < positionKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", positionKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No translation keys on this track.");
            }

            if (rotationKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Quaternion sampled =
                    SampleDebugAnimationRotationKeys(track->RotationKeys, sampleTime, &fromKey, &toKey, &alpha);
                const Vector3 sampledEuler = FMath::degrees(FMath::eulerAngles(sampled));

                ImGui::Text("Asset Track Rotation Q (Sampled): %.3f  %.3f  %.3f  %.3f",
                    sampled.w, sampled.x, sampled.y, sampled.z);
                ImGui::Text("Asset Track Rotation E (Sampled): %.3f  %.3f  %.3f",
                    sampledEuler.x, sampledEuler.y, sampledEuler.z);

                if (hasRuntimePoseData)
                {
                    const float localAngleDelta = QuaternionAngularDifferenceDegrees(sampled, runtimeLocalR);
                    const Vector3 localEulerDelta = sampledEuler - runtimeLocalEuler;

                    ImGui::Text("Delta vs Runtime Local Q: %.3f deg", localAngleDelta);
                    ImGui::Text("Delta vs Runtime Local E: %.3f  %.3f  %.3f",
                        localEulerDelta.x, localEulerDelta.y, localEulerDelta.z);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationQuatKey& from = track->RotationKeys[fromKey];
                    ImGui::Text("Rot From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationQuatKey& to = track->RotationKeys[toKey];
                        ImGui::Text("Rot To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Rotation Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        rotationKeyCount < maxShownKeys ? rotationKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationQuatKey& key = track->RotationKeys[i];
                        const Quaternion normalized = FMath::normalize(key.Value);
                        const Vector3 keyEuler = FMath::degrees(FMath::eulerAngles(normalized));
                        ImGui::Text(
                            "#%d  t=%.3f  q=(%.3f, %.3f, %.3f, %.3f)  e=(%.3f, %.3f, %.3f)",
                            i,
                            key.TimeSeconds,
                            normalized.w, normalized.x, normalized.y, normalized.z,
                            keyEuler.x, keyEuler.y, keyEuler.z);
                    }

                    if (shownKeyCount < rotationKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", rotationKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No rotation keys on this track.");
            }

            if (scaleKeyCount > 0)
            {
                int32 fromKey = -1;
                int32 toKey = -1;
                float alpha = 0.0f;
                const Vector3 sampled =
                    SampleDebugAnimationVectorKeys(track->ScaleKeys, sampleTime, &fromKey, &toKey, &alpha);

                ImGui::Text("Asset Track Scale (Sampled): %.3f  %.3f  %.3f", sampled.x, sampled.y, sampled.z);

                if (hasRuntimePoseData)
                {
                    const Vector3 localScaleDelta = sampled - runtimeLocalS;
                    const float localScaleDeltaLen = FMath::length(localScaleDelta);
                    ImGui::Text("Delta vs Runtime Local S: %.3f  %.3f  %.3f (len %.3f)",
                        localScaleDelta.x,
                        localScaleDelta.y,
                        localScaleDelta.z,
                        localScaleDeltaLen);
                }

                if (fromKey >= 0 && toKey >= 0)
                {
                    const AnimationVecKey& from = track->ScaleKeys[fromKey];
                    ImGui::Text("Scale From Key #%d @ %.3f", fromKey, from.TimeSeconds);

                    if (toKey != fromKey)
                    {
                        const AnimationVecKey& to = track->ScaleKeys[toKey];
                        ImGui::Text("Scale To Key   #%d @ %.3f (alpha %.3f)", toKey, to.TimeSeconds, alpha);
                    }
                }

                if (ImGui::TreeNode("Scale Keys (Raw Asset Data)"))
                {
                    const int32 maxShownKeys = 128;
                    const int32 shownKeyCount =
                        scaleKeyCount < maxShownKeys ? scaleKeyCount : maxShownKeys;

                    for (int32 i = 0; i < shownKeyCount; ++i)
                    {
                        const AnimationVecKey& key = track->ScaleKeys[i];
                        ImGui::Text(
                            "#%d  t=%.3f  s=(%.3f, %.3f, %.3f)",
                            i, key.TimeSeconds, key.Value.x, key.Value.y, key.Value.z);
                    }

                    if (shownKeyCount < scaleKeyCount)
                    {
                        ImGui::TextDisabled("... %d more keys", scaleKeyCount - shownKeyCount);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextDisabled("No scale keys on this track.");
            }
        }
    }

    ImGui::End();
}

void ImGuiModule::Tick(float dt)
{
    BeginFrame();
    
    
    // --- Main Menu Bar ---
    DrawTitleBar();
    
    // --- Root Dock space ---
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Level-editor-only tab style
    ImGui::PushStyleVar(ImGuiStyleVar_TabBorderSize,    2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding,      6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_TabBarBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     ImVec2(10, 8));

    ImGui::PushStyleColor(ImGuiCol_Tab,        ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.30f, 0.32f, 0.36f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive,  ImVec4(0.25f, 0.27f, 0.30f, 1.0f)); 
    
    
    ImGuiViewport* vp = ImGui::GetMainViewport();

    // Root dockspace window **under** the title bar
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + barHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - barHeight+1));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;


    ImGui::Begin("Editor Root###EditorRoot", nullptr, rootFlags);

    // This is your **ROOT** dockspace
    ImGuiID rootDockspaceID = ImGui::GetID("RootDockspace");
    ImGui::DockSpace(rootDockspaceID, ImVec2(0, 0));

    ImGui::End();
    
    // use this syles only for Asset Editors
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(4);        // tab vars
    ImGui::PopStyleVar(2);        // window padding/border
    
    
    // --- Level Editor
    ImGuiWindowFlags levelEditorFlags =
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove;
    
    
    ImGui::Begin("Level Editors###Level Editor",nullptr,levelEditorFlags);
    ImGuiID levelEditorDockspaceID = ImGui::GetID("LevelEditorDockspace");
    ImVec2 dockSize = ImGui::GetContentRegionAvail();
    ImGui::DockSpace(levelEditorDockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
    
    static bool firstLayout = true;
    if (firstLayout)
    {
        firstLayout = false;
        BuildLevelEditorLayout(levelEditorDockspaceID, dockSize);
    }
    

    DrawLevelEditorToolbar();
    //ImGui::ShowDemoWindow();
    
    // --- Viewport ---
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags viewportFlags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    
    if (ImGui::Begin("Viewport###Viewport", nullptr, viewportFlags))
    {
        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        const int32 width = (int32)(viewportSize.x);
        const int32 height = (int32)(viewportSize.y);
        if (RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>())
        {
            renderer->SetViewportSize(width, height);
            GLuint tex = renderer->GetViewportTexture();
            ImVec2 imagePos = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t)tex, viewportSize);
            
            ImGui::SetItemAllowOverlap();   // allow gizmo to be on top
            DrawViewportGizmo(imagePos, viewportSize);
            ImGuiIO& io = ImGui::GetIO();
            inside =
                io.MousePos.x >= imagePos.x &&
                io.MousePos.x <= imagePos.x + viewportSize.x &&
                io.MousePos.y >= imagePos.y &&
                io.MousePos.y <= imagePos.y + viewportSize.y;

            ImGui::GetForegroundDrawList()->AddText(
                ImVec2(imagePos.x + 10, imagePos.y + 10),
                inside ? IM_COL32(0,255,0,255) : IM_COL32(255,0,0,255),
                inside ? "INSIDE VIEWPORT" : "OUTSIDE VIEWPORT"
            );

            if (!bIsGizmoActive && inside)
            {
                entt::entity hit = entt::null;
                if (HandleViewportPicking(renderer, imagePos, viewportSize, hit))
                {
                    RB_LOG(EditorGUI, info, "Selected actor {}", (uint32)hit);
                    GSelectedActor = GEngine->GetActiveScene()->GetActor(hit);
                }
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    // optional: clear selection only when clicking inside viewport but hitting nothing
                    // (if you want, I can add a "clickedInsideViewport" out param)
                    RB_LOG(EditorGUI, info, "Nothing hit!");
                    GSelectedActor = nullptr;
                }
            }

            AssetHandle testMeshID;
            
            for (auto& pair : GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetRegistry().GetAll())
            {
                if (pair.Value.Path == "assets/Test.mesh")
                {
                    testMeshID = pair.Value.ID;
                    break;
                }
            }

            
            /*if (!TestMesh)
                TestMesh = (MeshAsset*)GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetManager().Load(testMeshID);*/

            /*if (TestMesh)
            {
                auto a = (OpenGLRenderAPI*)(renderer->GetRendererAPI());
                a->SubmitDraw(TestMesh->Handle, Mat4(1.0f), 0, 1);
            }*/

        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2); // border + padding
    
    
    DrawConsole();
    DrawAnimationImporterWindow();
    DrawAnimationDebuggerWindow();

    // --- Content Browser
    ImGui::Begin("Content Browser###Content Browser");

    /*static String CurentDirectory = "assets/";

    const auto& registredAssets = GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetRegistry().GetAll();
    for (auto assetPair : registredAssets)
    {
        auto assetMeta = assetPair.Value;
        ImGui::Button(assetMeta.Path.c_str());
    }*/

    static String CurrentDirectory = "Assets";

    TArray<String> folders;
    TArray<const AssetMeta*> files;

    const auto& registry =
        GEngine->GetModuleManager()
            .GetModule<AssetManagerModule>()
            ->GetRegistry()
            .GetAll();
    
    // !!&!! "Refs its always Refs"

    /**
     * The explanation is that if you remove the "&" from the auto
     * since assetPair is a stack variable its location its always same for the loop.
     * Because of that meta ptr will always point to the same location
     * result of every variable in the "files" array to be the same 
     */
    
    for (auto& assetPair : registry)
    {
        const AssetMeta& meta = assetPair.Value;
        const String& path = meta.Path;

        // Only consider assets under this directory
        if (!Rebel::Core::StartsWith(path, CurrentDirectory))
            continue;

        // Remove "Assets"
        String relative = path.Substr(CurrentDirectory.length(), path.length() - CurrentDirectory.length());

        // Remove leading '/'
        if (relative.length() > 0 && relative[0] == '/')
            relative = relative.Substr(1, relative.length() - 1);

        if (relative.length() == 0)
            continue;

        // Split by /
        auto parts = Rebel::Core::Split(relative, '/');

        if (parts.Num() == 1)
        {
            // File in this folder
            files.Add(&meta); // calls Add(const T&)
            //DEBUG_BREAK();
        }
        else
        {
            // In a subfolder → collect folder name
            const String& folder = parts[0];

            bool exists = false;
            for (auto& f : folders)
            {
                if (f == folder)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                folders.Add(folder);
        }
    }

    // Back button

        if (ImGui::Button("<-"))
        {
            auto parts = Rebel::Core::Split(CurrentDirectory, '/');
            parts.PopBack();
            CurrentDirectory = Rebel::Core::Join(parts, "/");
            if (CurrentDirectory.length() == 0)
                CurrentDirectory = "Assets";
        }
    

    ImGui::Separator();

    // Draw folders
    for (auto& folder : folders)
    {
        String label = "[ " + folder + " ]";
        if (ImGui::Button(label.c_str()))
        {
            CurrentDirectory += "/";
            CurrentDirectory += folder;
        }
        ImGui::SameLine();
    }

    // Draw files
    for (auto* meta : files)
    {
        auto parts = Rebel::Core::Split(meta->Path, '/');
        String name = parts[parts.Num() - 1];

        ImGui::Button(name.c_str());
        ImGui::SameLine();
    }


    
    ImGui::End();


    ImGui::Begin("Outliner###Outliner", nullptr);
    
        Scene* scene = GEngine->GetActiveScene();
        if (scene)
        {
            auto& registry = scene->GetRegistry();            // <--- important
            auto view = registry.view<SceneComponent*>(); 
    
            for (auto entity : view)
            {
                auto& sc = view.get<SceneComponent*>(entity);
    
                // Only draw roots (no parent)
                DrawActorNode(sc->Owner->GetHandle(), registry, *sc->Owner);
            }
        }
        
    ImGui::End();

    // --- Details
    ImGui::Begin("Details###Details",&a);

    Actor* selected = GSelectedActor;
    if (!selected || !selected->IsValid())
    {
        ImGui::TextDisabled("No Actor selected.");
    }
    else
    {
        // Name field at the top
        if (selected->HasComponent<NameComponent>())
        {
            auto& nameComp = selected->GetComponent<NameComponent>();

            char buffer[256];
            strncpy_s(buffer, nameComp.Name.c_str(), sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            if (ImGui::InputText(" ", buffer, sizeof(buffer)))
                nameComp.Name = buffer;
        }

        ImGui::Separator();

        DrawComponentHierarchy(GEngine->GetActiveScene()->GetRegistry(), selected->GetHandle(),*selected);

        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponentPopup");
        
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            auto& reg = selected->GetScene()->GetRegistry();
            entt::entity e = selected->GetHandle();
        
            for (auto& info : ComponentRegistry::Get().GetComponents())
            {
                // Skip SceneComponent if you want
                if (info.Name == "SceneComponent" || info.Name == "TagComponent"||info.Name == "ActorTagComponent")
                    continue;
        
                bool has = info.HasFn(*selected);
        
                if (has)
                {
                    // Disable existing components
                    ImGui::BeginDisabled();
                    ImGui::MenuItem(info.Name.c_str());
                    ImGui::EndDisabled();
                }
                else
                {
                    if (ImGui::MenuItem(info.Name.c_str()))
                    {
                        AddComponentToEntity(*selected, info);
                        RB_LOG(ImGuiLog, info, "Added component %s", info.Name.c_str());
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        
            ImGui::EndPopup();
        }


        ImGui::Separator();
        // Instead of manual DrawComponentByReflection<...> calls:
        DrawComponentsForActor(*selected);
    }
    
    ImGui::End();

/*#pragma region graph
    // --- Graph Editor ---
    ImGui::Begin("Graph###GraphEditor");
    DrawGraphCanvas();
    ImGui::End();

#pragma endregion */


    EndFrame();
}



void ImGuiModule::OnEvent(const Event& e)
{
    /*ImGuiIO& io = ImGui::GetIO();

    switch (e.Type)
    {
        case Event::Type::MouseButtonPressed:
            if (e.Key < IM_ARRAYSIZE(io.MouseDown))
                io.MouseDown[e.Key] = true;
            break;
        case Event::Type::MouseButtonReleased:
            if (e.Key < IM_ARRAYSIZE(io.MouseDown))
                io.MouseDown[e.Key] = false;
            break;
        case Event::Type::MouseMoved:
        {
            float sx, sy;
            GLFWwindow* w = GEngine->GetWindow()->GetGLFWWindow();
            glfwGetWindowContentScale(w, &sx, &sy);   // DPI scale

            io.MousePos = ImVec2(
                (float)e.X * sx,
                (float)e.Y * sy
            );
            break;
        }
        case Event::Type::MouseScrolled:
        {
            io.MouseWheel = e.ScrollY;   // do NOT accumulate
            break;
        }
        case Event::Type::WindowResize:
            io.DisplaySize = ImVec2((float)e.X, (float)e.Y);
            break;
        default:
            break;
    }*/
}


void ImGuiModule::Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    
    RB_LOG(ImGuiLog, info, "Shutdown ImGuiModule");
}

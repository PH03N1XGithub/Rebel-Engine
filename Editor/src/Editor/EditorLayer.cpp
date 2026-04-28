#include "Editor/EditorLayer.h"

#include "Editor/UI/EditorImGui.h"
#include "EditorEngine.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Framework/Window.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cctype>
#include <filesystem>

namespace
{
constexpr char kRootWindowName[] = "DockSpace Demo###EditorDockSpace";
constexpr char kShellDocumentDockspaceName[] = "EditorShellDocumentDockspace";
constexpr char kGlobalContentBrowserWindowName[] = "Content Browser###GlobalContentBrowser";
constexpr ImGuiID kShellDocumentClassId = 0x53484443;

constexpr float kTitlebarHeight = 54.0f;
constexpr float kTitlebarButtonSize = 18.0f;
constexpr float kTitlebarButtonFrame = 28.0f;
constexpr float kResizeBorderThickness = 6.0f;
constexpr int kMinimumWindowWidth = 960;
constexpr int kMinimumWindowHeight = 540;

bool IsMouseInRect(const ImVec2& min, const ImVec2& max)
{
    const ImVec2 mouse = ImGui::GetMousePos();
    return mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y;
}

bool DrawTitlebarButton(const char* id, const char* icon, const ImVec2& pos, const ImVec2& size, bool danger = false)
{
    ImGui::SetCursorScreenPos(pos);

    const ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    const ImVec4 hover = danger ? ImVec4(0.55f, 0.18f, 0.18f, 1.0f) : ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    const ImVec4 active = danger ? ImVec4(0.68f, 0.20f, 0.20f, 1.0f) : ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((size.x - kTitlebarButtonSize) * 0.5f, 4.0f));

    const bool pressed = ImGui::Button((std::string(icon) + "###" + id).c_str(), size);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

EditorWindowResizeRegion GetResizeRegion(const ImVec2& min, const ImVec2& max)
{
    const ImVec2 mouse = ImGui::GetMousePos();
    if (mouse.x < min.x || mouse.x > max.x || mouse.y < min.y || mouse.y > max.y)
        return EditorWindowResizeRegion::None;

    const bool left = mouse.x <= min.x + kResizeBorderThickness;
    const bool right = mouse.x >= max.x - kResizeBorderThickness;
    const bool top = mouse.y <= min.y + kResizeBorderThickness;
    const bool bottom = mouse.y >= max.y - kResizeBorderThickness;

    if (top && left)
        return EditorWindowResizeRegion::TopLeft;
    if (top && right)
        return EditorWindowResizeRegion::TopRight;
    if (bottom && left)
        return EditorWindowResizeRegion::BottomLeft;
    if (bottom && right)
        return EditorWindowResizeRegion::BottomRight;
    if (left)
        return EditorWindowResizeRegion::Left;
    if (right)
        return EditorWindowResizeRegion::Right;
    if (top)
        return EditorWindowResizeRegion::Top;
    if (bottom)
        return EditorWindowResizeRegion::Bottom;

    return EditorWindowResizeRegion::None;
}

ImGuiMouseCursor GetResizeCursor(EditorWindowResizeRegion region)
{
    switch (region)
    {
    case EditorWindowResizeRegion::Left:
    case EditorWindowResizeRegion::Right:
        return ImGuiMouseCursor_ResizeEW;
    case EditorWindowResizeRegion::Top:
    case EditorWindowResizeRegion::Bottom:
        return ImGuiMouseCursor_ResizeNS;
    case EditorWindowResizeRegion::TopLeft:
    case EditorWindowResizeRegion::BottomRight:
        return ImGuiMouseCursor_ResizeNWSE;
    case EditorWindowResizeRegion::TopRight:
    case EditorWindowResizeRegion::BottomLeft:
        return ImGuiMouseCursor_ResizeNESW;
    default:
        return ImGuiMouseCursor_Arrow;
    }
}

const EditorPanelDescriptor* FindPanelDescriptor(const EditorWorkspace& workspace, EditorPanelKind kind)
{
    for (const EditorPanelDescriptor& panel : workspace.Panels)
    {
        if (panel.Kind == kind)
            return &panel;
    }

    return nullptr;
}

ImGuiID GetWindowDockspaceRootId(const char* windowName)
{
    ImGuiWindow* window = ImGui::FindWindowByName(windowName);
    if (!window || !window->DockNode)
        return 0;

    ImGuiDockNode* rootNode = window->DockNode;
    while (rootNode->ParentNode)
        rootNode = rootNode->ParentNode;

    return rootNode ? rootNode->ID : 0;
}

bool IsSupportedDroppedImportPath(const String& path)
{
    std::filesystem::path fsPath(path.c_str());
    String extension = fsPath.extension().string().c_str();
    for (size_t i = 0; i < extension.length(); ++i)
        extension[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));

    return extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".obj";
}

const char* GetDroppedImportKindLabel(const DroppedImportKind kind)
{
    switch (kind)
    {
    case DroppedImportKind::StaticMesh:
        return "Static Mesh";
    case DroppedImportKind::SkeletalMesh:
        return "Skeletal Mesh";
    case DroppedImportKind::Animation:
        return "Animation";
    default:
        return "Static Mesh";
    }
}
}

EditorLayer::EditorLayer()
    : m_Selection(GetEditorSelection())
    , m_LevelToolbar(m_Selection)
    , m_Outliner(m_Selection)
    , m_Details(m_Selection)
    , m_Viewport(m_Selection)
    , m_AnimationDebugger(m_Selection)
{
    EditorWorkspace& levelEditor = m_Workspaces[0];
    levelEditor.Layer = EditorDockLayer::ShellWorkspace;
    levelEditor.Kind = EditorWorkspaceKind::LevelEditor;
    levelEditor.Label = "Level Editor";
    levelEditor.WorkspaceWindowName = "Level Editor###LevelEditorWorkspace";
    levelEditor.HostChildName = "##LevelEditorWorkspaceHost";
    levelEditor.DockspaceName = "LevelEditorDockspace";
    levelEditor.WorkspaceClassId = kShellDocumentClassId;
    levelEditor.DockspaceClassId = ImHashStr(levelEditor.DockspaceName);
    levelEditor.Panels = { {
        { EditorDockLayer::EditorScopedPanel, "Toolbar###LevelEditorToolbar", EditorPanelKind::Toolbar, true },
        { EditorDockLayer::EditorScopedPanel, "Viewport###LevelEditorViewport", EditorPanelKind::Viewport, false },
        { EditorDockLayer::EditorScopedPanel, "Scene Hierarchy###LevelEditorOutliner", EditorPanelKind::Outliner, false },
        { EditorDockLayer::EditorScopedPanel, "Properties###LevelEditorDetails", EditorPanelKind::Details, false },
    } };

    m_GlobalUtilityTabs = { {
        { EditorDockLayer::GlobalUtilityTab, kGlobalContentBrowserWindowName, GlobalUtilityTabKind::ContentBrowser, true },
        { EditorDockLayer::GlobalUtilityTab, "Console", GlobalUtilityTabKind::Console, true },
        { EditorDockLayer::GlobalUtilityTab, "Game Mode", GlobalUtilityTabKind::GameMode, true },
        { EditorDockLayer::GlobalUtilityTab, "Animation Debugger", GlobalUtilityTabKind::AnimationDebugger, true },
        { EditorDockLayer::GlobalUtilityTab, "Static Mesh Importer", GlobalUtilityTabKind::StaticMeshImporter, false },
        { EditorDockLayer::GlobalUtilityTab, "Skeletal Mesh Importer", GlobalUtilityTabKind::SkeletalMeshImporter, false },
        { EditorDockLayer::GlobalUtilityTab, "Animation Importer", GlobalUtilityTabKind::AnimationImporter, false },
    } };

    m_MenuBar.SetAnimationImporterVisibility(m_AnimationImporter.GetVisibilityPtr());
    m_MenuBar.SetStaticMeshImporterVisibility(m_StaticMeshImporter.GetVisibilityPtr());
    m_MenuBar.SetSkeletalMeshImporterVisibility(m_SkeletalMeshImporter.GetVisibilityPtr());
    m_Shortcuts.SetToggleContentBrowserAction([this]() { ToggleContentBrowserVisibility(); });

    m_AssetEditorManager.RegisterEditor(m_PrefabEditor);
    m_AssetEditorManager.RegisterEditor(m_AnimationAssetEditor);
    m_AssetEditorManager.RegisterEditor(m_SkeletonAssetEditor);
    m_AssetEditorManager.RegisterEditor(m_SkeletalMeshAssetEditor);
    m_AssetEditorManager.RegisterEditor(m_AnimGraphAssetEditor);
}

void EditorLayer::TickShortcuts()
{
    m_Shortcuts.Tick();
}

void EditorLayer::ToggleContentBrowserVisibility()
{
    m_ShowContentBrowser = !m_ShowContentBrowser;
}

void EditorLayer::Draw(float dt)
{
    CollectDroppedFiles();
    DrawRootShell();
    DrawOuterDockspace();
    DrawWorkspace(GetActiveWorkspace(), dt);
    DrawGlobalUtilityWindows();
    DrawDroppedImportPopup();
    m_AssetEditorManager.DrawOpenEditors(m_ShellDocumentDockId, kShellDocumentClassId);
}

void EditorLayer::CollectDroppedFiles()
{
    Window* window = GEngine ? GEngine->GetWindow() : nullptr;
    if (!window)
        return;

    TArray<String> droppedFiles = window->ConsumeDroppedFiles();
    if (droppedFiles.Num() == 0)
        return;

    m_DroppedImportFiles.Clear();
    for (int32 i = 0; i < droppedFiles.Num(); ++i)
    {
        if (IsSupportedDroppedImportPath(droppedFiles[i]))
            m_DroppedImportFiles.Add(droppedFiles[i]);
    }

    if (m_DroppedImportFiles.Num() == 0)
        return;

    m_DroppedImportStatus = "";
    m_ShowDroppedImportPopup = true;
    m_OpenDroppedImportPopupNextFrame = true;
}

void EditorLayer::DrawRootShell()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    Window* window = GEngine ? GEngine->GetWindow() : nullptr;
    const bool isMaximized = window && window->IsMaximized();

    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
    ImGui::Begin(kRootWindowName, nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    if (!isMaximized)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiWindow* currentWindow = ImGui::GetCurrentWindow();
        const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
        const ImVec2 min = currentWindow->Pos;
        const ImVec2 max(
            currentWindow->Pos.x + currentWindow->Size.x,
            currentWindow->Pos.y + currentWindow->Size.y);
        drawList->AddLine(ImVec2(min.x, min.y + 1.0f), ImVec2(min.x, max.y), borderColor, 1.0f);
        drawList->AddLine(ImVec2(max.x - 1.0f, min.y + 1.0f), ImVec2(max.x - 1.0f, max.y), borderColor, 1.0f);
        drawList->AddLine(ImVec2(min.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), borderColor, 1.0f);
        HandleWindowResize(min, max);
    }
    else
    {
        m_WindowResizeActive = false;
        m_WindowResizeRegion = EditorWindowResizeRegion::None;
    }

    const float titlebarHeight = DrawTitlebar();
    ImGui::SetCursorPosY(titlebarHeight + ImGui::GetCurrentWindow()->WindowPadding.y);
}

float EditorLayer::DrawTitlebar()
{
    Window* window = GEngine ? GEngine->GetWindow() : nullptr;
    const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

    ImGui::SetCursorPos(windowPadding);
    const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
    const ImVec2 titlebarMax(
        titlebarMin.x + ImGui::GetWindowWidth() - windowPadding.x * 2.0f,
        titlebarMin.y + kTitlebarHeight);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(titlebarMin, titlebarMax, ImGui::GetColorU32(ImGuiCol_TitleBg));
    drawList->AddRectFilledMultiColor(
        titlebarMin,
        ImVec2(titlebarMin.x + 360.0f, titlebarMax.y),
        ImGui::GetColorU32(ImVec4(140.0f / 255.0f, 20.0f / 255.0f, 25.0f / 255.0f, 0.28f)),
        ImGui::GetColorU32(ImGuiCol_TitleBg),
        ImGui::GetColorU32(ImGuiCol_TitleBg),
        ImGui::GetColorU32(ImVec4(90.0f / 255.0f, 15.0f / 255.0f, 18.0f / 255.0f, 0.28f)));
    drawList->AddLine(
        ImVec2(titlebarMin.x, titlebarMax.y),
        ImVec2(titlebarMax.x, titlebarMax.y),
        ImGui::GetColorU32(ImGuiCol_Border),
        1.0f);

    bool menuHovered = false;
    bool controlsHovered = false;
    DrawTitlebarMenu(titlebarMin, kTitlebarHeight, menuHovered);
    DrawTitlebarControls(titlebarMin, kTitlebarHeight, controlsHovered);

    {
        Rebel::Editor::UI::ScopedFont titleFont(Rebel::Editor::UI::GetFont("BoldTitle"));
        const char* title = "Rebel Editor";
        const ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorScreenPos(ImVec2(
            titlebarMin.x + (titlebarMax.x - titlebarMin.x) * 0.5f - titleSize.x * 0.5f,
            titlebarMin.y + 8.0f));
        ImGui::TextUnformatted(title);
    }

    {
        const char* sceneLabel = "scene.Ryml";
        if (GEngine && GEngine->IsPlaying())
        {
            sceneLabel = "TempPIE.Ryml";
        }
        else if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
        {
            const String& currentScenePath = editor->GetCurrentScenePath();
            if (currentScenePath.length() > 0)
                sceneLabel = currentScenePath.c_str();
        }

        const char* modeLabel = GEngine && GEngine->IsPlaying() ? "Play Mode" : "Edit Mode";

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.73f, 1.0f));
        const ImVec2 sceneSize = ImGui::CalcTextSize(sceneLabel);
        const ImVec2 modeSize = ImGui::CalcTextSize(modeLabel);
        ImGui::SetCursorScreenPos(ImVec2(titlebarMax.x - 180.0f - sceneSize.x, titlebarMin.y + 8.0f));
        ImGui::TextUnformatted(sceneLabel);
        ImGui::SetCursorScreenPos(ImVec2(titlebarMax.x - 180.0f - modeSize.x, titlebarMin.y + 28.0f));
        ImGui::TextUnformatted(modeLabel);
        ImGui::PopStyleColor();
    }

    const bool titlebarInteractiveHover =
        IsMouseInRect(titlebarMin, titlebarMax) &&
        !menuHovered &&
        !controlsHovered &&
        !m_WindowResizeActive &&
        !(window && window->IsCursorDisabled());

    m_TitlebarHovered = titlebarInteractiveHover;

    if (window)
    {
        GLFWwindow* glfwWindow = window->GetGLFWWindow();

        if (titlebarInteractiveHover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (window->IsMaximized())
                window->Restore();
            else
                window->Maximize();
        }
        else if (titlebarInteractiveHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int windowX = 0;
            int windowY = 0;
            int windowWidth = 0;
            int windowHeight = 0;
            glfwGetWindowPos(glfwWindow, &windowX, &windowY);
            glfwGetWindowSize(glfwWindow, &windowWidth, &windowHeight);

            const ImVec2 mousePos = ImGui::GetMousePos();
            m_WindowDragActive = true;
            m_WindowDragStartedMaximized = window->IsMaximized();
            m_WindowDragOffset = ImVec2(mousePos.x - (float)windowX, mousePos.y - (float)windowY);
            m_WindowDragRestoreRatio =
                (windowWidth > 0) ? ImClamp((mousePos.x - (float)windowX) / (float)windowWidth, 0.0f, 1.0f) : 0.5f;
        }

        if (m_WindowDragActive)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_WindowDragActive = false;
                m_WindowDragStartedMaximized = false;
            }
            else
            {
                const ImVec2 mousePos = ImGui::GetMousePos();

                if (m_WindowDragStartedMaximized && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
                {
                    window->Restore();

                    int restoredWidth = 0;
                    int restoredHeight = 0;
                    glfwGetWindowSize(glfwWindow, &restoredWidth, &restoredHeight);

                    const int targetX = (int)(mousePos.x - (float)restoredWidth * m_WindowDragRestoreRatio);
                    const int targetY = (int)(mousePos.y - m_WindowDragOffset.y);
                    glfwSetWindowPos(glfwWindow, targetX, targetY);

                    m_WindowDragOffset = ImVec2(mousePos.x - (float)targetX, mousePos.y - (float)targetY);
                    m_WindowDragStartedMaximized = false;
                }
                else if (!m_WindowDragStartedMaximized && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
                {
                    const int targetX = (int)(mousePos.x - m_WindowDragOffset.x);
                    const int targetY = (int)(mousePos.y - m_WindowDragOffset.y);
                    glfwSetWindowPos(glfwWindow, targetX, targetY);
                }
            }
        }
    }

    return kTitlebarHeight;
}

void EditorLayer::HandleWindowResize(const ImVec2& windowMin, const ImVec2& windowMax)
{
    Window* window = GEngine ? GEngine->GetWindow() : nullptr;
    if (!window || window->IsMaximized() || window->IsCursorDisabled())
    {
        m_WindowResizeActive = false;
        m_WindowResizeRegion = EditorWindowResizeRegion::None;
        return;
    }

    GLFWwindow* glfwWindow = window->GetGLFWWindow();
    if (!glfwWindow)
        return;

    if (!m_WindowResizeActive)
    {
        m_WindowResizeRegion = GetResizeRegion(windowMin, windowMax);
        if (m_WindowResizeRegion != EditorWindowResizeRegion::None)
            ImGui::SetMouseCursor(GetResizeCursor(m_WindowResizeRegion));

        if (m_WindowResizeRegion != EditorWindowResizeRegion::None &&
            !m_WindowDragActive &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int windowX = 0;
            int windowY = 0;
            int windowWidth = 0;
            int windowHeight = 0;
            glfwGetWindowPos(glfwWindow, &windowX, &windowY);
            glfwGetWindowSize(glfwWindow, &windowWidth, &windowHeight);

            m_WindowResizeActive = true;
            m_WindowResizeStartMouse = ImGui::GetMousePos();
            m_WindowResizeStartPos = ImVec2((float)windowX, (float)windowY);
            m_WindowResizeStartSize = ImVec2((float)windowWidth, (float)windowHeight);
        }

        return;
    }

    ImGui::SetMouseCursor(GetResizeCursor(m_WindowResizeRegion));

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_WindowResizeActive = false;
        m_WindowResizeRegion = EditorWindowResizeRegion::None;
        return;
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const ImVec2 mouseDelta(
        mousePos.x - m_WindowResizeStartMouse.x,
        mousePos.y - m_WindowResizeStartMouse.y);

    int targetX = (int)m_WindowResizeStartPos.x;
    int targetY = (int)m_WindowResizeStartPos.y;
    int targetWidth = (int)m_WindowResizeStartSize.x;
    int targetHeight = (int)m_WindowResizeStartSize.y;

    const bool resizeLeft =
        m_WindowResizeRegion == EditorWindowResizeRegion::Left ||
        m_WindowResizeRegion == EditorWindowResizeRegion::TopLeft ||
        m_WindowResizeRegion == EditorWindowResizeRegion::BottomLeft;
    const bool resizeRight =
        m_WindowResizeRegion == EditorWindowResizeRegion::Right ||
        m_WindowResizeRegion == EditorWindowResizeRegion::TopRight ||
        m_WindowResizeRegion == EditorWindowResizeRegion::BottomRight;
    const bool resizeTop =
        m_WindowResizeRegion == EditorWindowResizeRegion::Top ||
        m_WindowResizeRegion == EditorWindowResizeRegion::TopLeft ||
        m_WindowResizeRegion == EditorWindowResizeRegion::TopRight;
    const bool resizeBottom =
        m_WindowResizeRegion == EditorWindowResizeRegion::Bottom ||
        m_WindowResizeRegion == EditorWindowResizeRegion::BottomLeft ||
        m_WindowResizeRegion == EditorWindowResizeRegion::BottomRight;

    if (resizeLeft)
    {
        targetX = (int)(m_WindowResizeStartPos.x + mouseDelta.x);
        targetWidth = (int)(m_WindowResizeStartSize.x - mouseDelta.x);
        if (targetWidth < kMinimumWindowWidth)
        {
            targetX -= (kMinimumWindowWidth - targetWidth);
            targetWidth = kMinimumWindowWidth;
        }
    }
    else if (resizeRight)
    {
        targetWidth = (int)(m_WindowResizeStartSize.x + mouseDelta.x);
        targetWidth = ImMax(targetWidth, kMinimumWindowWidth);
    }

    if (resizeTop)
    {
        targetY = (int)(m_WindowResizeStartPos.y + mouseDelta.y);
        targetHeight = (int)(m_WindowResizeStartSize.y - mouseDelta.y);
        if (targetHeight < kMinimumWindowHeight)
        {
            targetY -= (kMinimumWindowHeight - targetHeight);
            targetHeight = kMinimumWindowHeight;
        }
    }
    else if (resizeBottom)
    {
        targetHeight = (int)(m_WindowResizeStartSize.y + mouseDelta.y);
        targetHeight = ImMax(targetHeight, kMinimumWindowHeight);
    }

    glfwSetWindowPos(glfwWindow, targetX, targetY);
    glfwSetWindowSize(glfwWindow, targetWidth, targetHeight);
}

void EditorLayer::DrawTitlebarMenu(const ImVec2& titlebarMin, float titlebarHeight, bool& menuHovered)
{
    const float menuWidth = ImGui::GetWindowWidth() * 0.42f;
    const ImVec2 menuPos(titlebarMin.x + 14.0f, titlebarMin.y + 8.0f);
    const ImVec2 menuSize(menuWidth, titlebarHeight - 14.0f);

    ImGui::SetCursorScreenPos(menuPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::BeginChild(
            "##TitlebarMenuHost",
            menuSize,
            ImGuiChildFlags_None,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        m_MenuBar.Draw();
        menuHovered = ImGui::IsAnyItemHovered();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorLayer::DrawTitlebarControls(const ImVec2& titlebarMin, float titlebarHeight, bool& controlsHovered)
{
    Window* window = GEngine ? GEngine->GetWindow() : nullptr;
    if (!window)
        return;

    const float rightPadding = 8.0f;
    const float y = titlebarMin.y + (titlebarHeight - kTitlebarButtonFrame) * 0.5f;
    float x = titlebarMin.x + ImGui::GetWindowWidth() - rightPadding - kTitlebarButtonFrame * 3.0f;

    if (DrawTitlebarButton("MinimizeWindow", ICON_FA_WINDOW_MINIMIZE, ImVec2(x, y), ImVec2(kTitlebarButtonFrame, kTitlebarButtonFrame)))
        window->Minimize();
    controlsHovered |= ImGui::IsItemHovered();
    x += kTitlebarButtonFrame;

    const char* maximizeIcon = window->IsMaximized() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE;
    if (DrawTitlebarButton("MaximizeWindow", maximizeIcon, ImVec2(x, y), ImVec2(kTitlebarButtonFrame, kTitlebarButtonFrame)))
    {
        if (window->IsMaximized())
            window->Restore();
        else
            window->Maximize();
    }
    controlsHovered |= ImGui::IsItemHovered();
    x += kTitlebarButtonFrame;

    if (DrawTitlebarButton("CloseWindow", ICON_FA_XMARK, ImVec2(x, y), ImVec2(kTitlebarButtonFrame, kTitlebarButtonFrame), true))
    {
        Event closeEvent{};
        closeEvent.Type = Event::Type::WindowClose;
        Window::GetEventDelegate().Broadcast(closeEvent);
    }
    controlsHovered |= ImGui::IsItemHovered();
}

void EditorLayer::DrawOuterDockspace()
{
    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    ImGui::BeginChild(
        "##ShellDocumentDockHost",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 documentSize = ImGui::GetContentRegionAvail();
    m_ShellDocumentDockId = ImGui::GetID(kShellDocumentDockspaceName);

    ImGuiWindowClass documentDockspaceClass{};
    documentDockspaceClass.ClassId = kShellDocumentClassId;
    documentDockspaceClass.DockingAllowUnclassed = false;
    documentDockspaceClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoDockingSplit;

    ImGuiStyle& style = ImGui::GetStyle();
    const float previousMinWidth = style.WindowMinSize.x;
    style.WindowMinSize.x = 240.0f;
    ImGui::DockSpace(m_ShellDocumentDockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_NoDockingSplit, &documentDockspaceClass);
    style.WindowMinSize.x = previousMinWidth;
    ImGui::EndChild();

    EnsureOuterLayout(m_ShellDocumentDockId, documentSize);
    ImGui::End();
}

void EditorLayer::DrawWorkspace(EditorWorkspace& workspace, float dt)
{
    if (m_ShellDocumentDockId != 0)
        ImGui::SetNextWindowDockID(m_ShellDocumentDockId, ImGuiCond_Always);

    const ImGuiID dockspaceID = ImGui::GetID(workspace.DockspaceName);
    ImGuiWindowClass workspaceWindowClass{};
    workspaceWindowClass.ClassId = workspace.WorkspaceClassId;
    workspaceWindowClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&workspaceWindowClass);

    const bool workspaceVisible = ImGui::Begin(
            workspace.WorkspaceWindowName,
            nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);

    ImGuiWindowClass workspaceDockspaceClass{};
    workspaceDockspaceClass.ClassId = workspace.DockspaceClassId;
    workspaceDockspaceClass.DockingAllowUnclassed = true;

    if (workspaceVisible)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild(
            workspace.HostChildName,
            ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();
        ImGuiStyle& style = ImGui::GetStyle();
        const float previousMinWidth = style.WindowMinSize.x;
        style.WindowMinSize.x = 240.0f;
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &workspaceDockspaceClass);
        style.WindowMinSize.x = previousMinWidth;

        EnsureWorkspaceLayout(workspace, dockspaceID, dockspaceSize);

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    else
    {
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly, &workspaceDockspaceClass);
    }
    ImGui::End();

    if (!workspaceVisible)
        return;

    ImGuiWindowClass workspacePanelClass{};
    workspacePanelClass.ClassId = workspace.DockspaceClassId;
    workspacePanelClass.DockingAllowUnclassed = false;

    ImGuiWindowClass toolbarWindowClass = workspacePanelClass;
    toolbarWindowClass.DockNodeFlagsOverrideSet =
        ImGuiDockNodeFlags_NoTabBar |
        ImGuiDockNodeFlags_NoWindowMenuButton |
        ImGuiDockNodeFlags_NoCloseButton;

    for (const EditorPanelDescriptor& panel : workspace.Panels)
    {
        ImGui::SetNextWindowClass(panel.ToolbarStyle ? &toolbarWindowClass : &workspacePanelClass);

        switch (panel.Kind)
        {
        case EditorPanelKind::Toolbar:
            if (ImGui::Begin(
                    panel.WindowName,
                    nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse))
            {
                m_LevelToolbar.Draw();
            }
            ImGui::End();
            break;

        case EditorPanelKind::Viewport:
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            if (ImGui::Begin(
                    panel.WindowName,
                    nullptr,
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse))
            {
                m_Viewport.SetDeltaTime(dt);
                m_Viewport.Draw();
            }
            ImGui::End();
            ImGui::PopStyleVar();
            break;

        case EditorPanelKind::Outliner:
            if (ImGui::Begin(panel.WindowName))
                m_Outliner.Draw();
            ImGui::End();
            break;

        case EditorPanelKind::Details:
            if (ImGui::Begin(panel.WindowName))
                m_Details.Draw();
            ImGui::End();
            break;
        }
    }
}

void EditorLayer::EnsureOuterLayout(ImGuiID documentDockId, const ImVec2& documentSize)
{
    if (m_OuterLayoutInitialized ||
        documentDockId == 0 ||
        documentSize.x <= 1.0f ||
        documentSize.y <= 1.0f)
        return;

    ImGuiDockNode* existingDocumentNode = ImGui::DockBuilderGetNode(documentDockId);
    if (existingDocumentNode)
    {
        existingDocumentNode->LocalFlags |= ImGuiDockNodeFlags_NoDockingSplit;
        m_OuterLayoutInitialized = true;
        return;
    }

    ImGui::DockBuilderRemoveNode(documentDockId);
    ImGui::DockBuilderAddNode(documentDockId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoDockingSplit);
    ImGui::DockBuilderSetNodeSize(documentDockId, documentSize);

    const EditorWorkspace& workspace = GetActiveWorkspace();
    ImGui::DockBuilderDockWindow(workspace.WorkspaceWindowName, documentDockId);

    if (ImGuiDockNode* documentNode = ImGui::DockBuilderGetNode(documentDockId))
        documentNode->LocalFlags |= ImGuiDockNodeFlags_NoDockingSplit;

    ImGui::DockBuilderFinish(documentDockId);
    m_OuterLayoutInitialized = true;
}

void EditorLayer::EnsureWorkspaceLayout(EditorWorkspace& workspace, ImGuiID dockspaceID, const ImVec2& size)
{
    if (workspace.LayoutInitialized || dockspaceID == 0 || size.x <= 1.0f || size.y <= 1.0f)
        return;

    ImGuiDockNode* existingNode = ImGui::DockBuilderGetNode(dockspaceID);
    if (existingNode && existingNode->ChildNodes[0] != nullptr)
    {
        workspace.LayoutInitialized = true;
        return;
    }

    const EditorPanelDescriptor* toolbar = FindPanelDescriptor(workspace, EditorPanelKind::Toolbar);
    const EditorPanelDescriptor* viewport = FindPanelDescriptor(workspace, EditorPanelKind::Viewport);
    const EditorPanelDescriptor* outliner = FindPanelDescriptor(workspace, EditorPanelKind::Outliner);
    const EditorPanelDescriptor* details = FindPanelDescriptor(workspace, EditorPanelKind::Details);

    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, size);

    ImGuiID dockToolbar = 0;
    ImGuiID dockLeft = 0;
    ImGuiID dockLeftBottom = 0;
    ImGuiID dockBottom = 0;
    ImGuiID dockMain = dockspaceID;

    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.08f, &dockToolbar, &dockMain);
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, &dockLeft, &dockMain);
    ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.50f, &dockLeftBottom, &dockLeft);
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, &dockBottom, &dockMain);

    if (toolbar)
        ImGui::DockBuilderDockWindow(toolbar->WindowName, dockToolbar);
    if (outliner)
        ImGui::DockBuilderDockWindow(outliner->WindowName, dockLeft);
    if (details)
        ImGui::DockBuilderDockWindow(details->WindowName, dockLeftBottom);
    if (viewport)
        ImGui::DockBuilderDockWindow(viewport->WindowName, dockMain);

    for (const GlobalUtilityTabDescriptor& utility : m_GlobalUtilityTabs)
    {
        if (utility.DockByDefault)
            ImGui::DockBuilderDockWindow(utility.WindowName, dockBottom);
    }

    if (ImGuiDockNode* toolbarNode = ImGui::DockBuilderGetNode(dockToolbar))
    {
        toolbarNode->LocalFlags |=
            ImGuiDockNodeFlags_NoTabBar |
            ImGuiDockNodeFlags_NoWindowMenuButton |
            ImGuiDockNodeFlags_NoCloseButton;
    }

    ImGui::DockBuilderFinish(dockspaceID);
    workspace.LayoutInitialized = true;
}

void EditorLayer::DrawGlobalUtilityWindows()
{
    const EditorWorkspace& workspace = GetActiveWorkspace();
    const ImGuiID workspaceDockspaceId = ImGui::GetID(workspace.DockspaceName);

    ImGuiWindowClass utilityWindowClass{};
    utilityWindowClass.ClassId = workspace.DockspaceClassId;

    auto beginUtilityWindow = [&](const char* windowName, bool* open = nullptr) -> bool
    {
        ImGui::SetNextWindowClass(&utilityWindowClass);
        if (workspaceDockspaceId != 0)
            ImGui::SetNextWindowDockID(workspaceDockspaceId, ImGuiCond_FirstUseEver);

        return ImGui::Begin(windowName, open);
    };

    if (m_ShowContentBrowser)
    {
        const bool contentBrowserVisible =
            beginUtilityWindow(m_GlobalUtilityTabs[0].WindowName, &m_ShowContentBrowser);
        if (contentBrowserVisible)
            m_ContentBrowser.Draw();

        ImGui::End();
    }

    if (const AssetHandle openAssetHandle = m_ContentBrowser.ConsumeOpenAssetRequest();
        IsValidAssetHandle(openAssetHandle))
    {
        m_AssetEditorManager.OpenAsset(openAssetHandle);
    }

    if (beginUtilityWindow(m_GlobalUtilityTabs[1].WindowName))
    {
        m_Debug.Draw();
    }
    ImGui::End();

    if (beginUtilityWindow(m_GlobalUtilityTabs[2].WindowName))
    {
        m_GameModePanel.Draw();
    }
    ImGui::End();

    if (beginUtilityWindow(m_GlobalUtilityTabs[3].WindowName))
    {
        m_AnimationDebugger.Draw();
    }
    ImGui::End();

    bool* staticImporterVisible = m_StaticMeshImporter.GetVisibilityPtr();
    if (*staticImporterVisible)
    {
        ImGui::SetNextWindowSize(ImVec2(760.0f, 260.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (beginUtilityWindow(m_GlobalUtilityTabs[4].WindowName, staticImporterVisible))
        {
            m_StaticMeshImporter.Draw();
        }
        ImGui::End();
    }

    bool* skeletalImporterVisible = m_SkeletalMeshImporter.GetVisibilityPtr();
    if (*skeletalImporterVisible)
    {
        ImGui::SetNextWindowSize(ImVec2(760.0f, 260.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (beginUtilityWindow(m_GlobalUtilityTabs[5].WindowName, skeletalImporterVisible))
        {
            m_SkeletalMeshImporter.Draw();
        }
        ImGui::End();
    }

    bool* animationImporterVisible = m_AnimationImporter.GetVisibilityPtr();
    if (*animationImporterVisible)
    {
        ImGui::SetNextWindowSize(ImVec2(760.0f, 260.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 220.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (beginUtilityWindow(m_GlobalUtilityTabs[6].WindowName, animationImporterVisible))
        {
            m_AnimationImporter.Draw();
        }
        ImGui::End();
    }
}

void EditorLayer::DrawDroppedImportPopup()
{
    if (!m_ShowDroppedImportPopup)
        return;

    if (m_OpenDroppedImportPopupNextFrame)
    {
        ImGui::OpenPopup("Import Dropped Files");
        m_OpenDroppedImportPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Import Dropped Files", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!ImGui::IsPopupOpen("Import Dropped Files"))
            m_ShowDroppedImportPopup = false;
        return;
    }

    ImGui::TextWrapped("Import %d dropped file(s) with one importer configuration.", m_DroppedImportFiles.Num());
    ImGui::Spacing();

    const char* currentKind = GetDroppedImportKindLabel(m_DroppedImportKind);
    if (ImGui::BeginCombo("Importer Type", currentKind))
    {
        const DroppedImportKind kinds[] = {
            DroppedImportKind::StaticMesh,
            DroppedImportKind::SkeletalMesh,
            DroppedImportKind::Animation
        };

        for (DroppedImportKind kind : kinds)
        {
            const bool selected = kind == m_DroppedImportKind;
            if (ImGui::Selectable(GetDroppedImportKindLabel(kind), selected))
                m_DroppedImportKind = kind;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::InputText("Output Directory", m_DroppedImportOutputDirectory, IM_ARRAYSIZE(m_DroppedImportOutputDirectory));

    if (m_DroppedImportKind == DroppedImportKind::Animation)
    {
        AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
        const auto& registry = assetModule->GetRegistry().GetAll();

        const char* skeletonLabel = "None";
        if ((uint64)m_DroppedImportSkeletonHandle != 0)
        {
            if (const AssetMeta* meta = registry.Find(m_DroppedImportSkeletonHandle))
                skeletonLabel = meta->Path.c_str();
        }

        if (ImGui::BeginCombo("Skeleton", skeletonLabel))
        {
            if (ImGui::Selectable("None", (uint64)m_DroppedImportSkeletonHandle == 0))
                m_DroppedImportSkeletonHandle = 0;

            for (const auto& pair : registry)
            {
                const AssetMeta& meta = pair.Value;
                if (meta.Type != SkeletonAsset::StaticType())
                    continue;

                const bool selected = meta.ID == m_DroppedImportSkeletonHandle;
                if (ImGui::Selectable(meta.Path.c_str(), selected))
                    m_DroppedImportSkeletonHandle = meta.ID;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    ImGui::Separator();
    ImGui::BeginChild("DroppedFilesList", ImVec2(680.0f, 180.0f), true);
    for (int32 i = 0; i < m_DroppedImportFiles.Num(); ++i)
        ImGui::TextWrapped("%s", m_DroppedImportFiles[i].c_str());
    ImGui::EndChild();

    if (m_DroppedImportStatus.length() > 0)
        ImGui::TextWrapped("%s", m_DroppedImportStatus.c_str());

    if (ImGui::Button("Import All", ImVec2(120.0f, 0.0f)))
    {
        int32 importedCount = 0;
        int32 importedClipCount = 0;
        String lastFailure;

        for (int32 i = 0; i < m_DroppedImportFiles.Num(); ++i)
        {
            const String& sourcePath = m_DroppedImportFiles[i];
            String status;

            if (m_DroppedImportKind == DroppedImportKind::StaticMesh)
            {
                if (StaticMeshImporterPanel::ImportFile(sourcePath, m_DroppedImportOutputDirectory, "", status))
                    ++importedCount;
                else
                    lastFailure = sourcePath + ": " + status;
            }
            else if (m_DroppedImportKind == DroppedImportKind::SkeletalMesh)
            {
                if (SkeletalMeshImporterPanel::ImportFile(sourcePath, m_DroppedImportOutputDirectory, "", status))
                    ++importedCount;
                else
                    lastFailure = sourcePath + ": " + status;
            }
            else
            {
                int32 savedClipCount = 0;
                if (AnimationImporterPanel::ImportFile(
                        sourcePath,
                        m_DroppedImportOutputDirectory,
                        m_DroppedImportSkeletonHandle,
                        savedClipCount,
                        status))
                {
                    ++importedCount;
                    importedClipCount += savedClipCount;
                }
                else
                {
                    lastFailure = sourcePath + ": " + status;
                }
            }
        }

        if (importedCount == m_DroppedImportFiles.Num())
        {
            if (m_DroppedImportKind == DroppedImportKind::Animation)
            {
                m_DroppedImportStatus =
                    String("Imported ") +
                    String(std::to_string(importedClipCount).c_str()) +
                    String(" clip(s) from ") +
                    String(std::to_string(importedCount).c_str()) +
                    String(" dropped file(s).");
            }
            else
            {
                m_DroppedImportStatus =
                    String("Imported ") +
                    String(std::to_string(importedCount).c_str()) +
                    String(" dropped file(s).");
            }
        }
        else
        {
            m_DroppedImportStatus =
                String("Imported ") +
                String(std::to_string(importedCount).c_str()) +
                String(" of ") +
                String(std::to_string(m_DroppedImportFiles.Num()).c_str()) +
                String(" file(s). ") +
                lastFailure;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Open Importer", ImVec2(120.0f, 0.0f)))
    {
        if (m_DroppedImportFiles.Num() > 0)
        {
            const String& firstPath = m_DroppedImportFiles[0];
            if (m_DroppedImportKind == DroppedImportKind::StaticMesh)
                m_StaticMeshImporter.OpenWithFile(firstPath, m_DroppedImportOutputDirectory);
            else if (m_DroppedImportKind == DroppedImportKind::SkeletalMesh)
                m_SkeletalMeshImporter.OpenWithFile(firstPath, m_DroppedImportOutputDirectory);
            else
                m_AnimationImporter.OpenWithFile(firstPath, m_DroppedImportOutputDirectory, m_DroppedImportSkeletonHandle);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        m_ShowDroppedImportPopup = false;
        m_DroppedImportFiles.Clear();
        m_DroppedImportStatus = "";
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

EditorWorkspace& EditorLayer::GetActiveWorkspace()
{
    const int clampedIndex = ImClamp(m_ActiveWorkspaceIndex, 0, (int)m_Workspaces.size() - 1);
    return m_Workspaces[(size_t)clampedIndex];
}

const EditorWorkspace& EditorLayer::GetActiveWorkspace() const
{
    const int clampedIndex = ImClamp(m_ActiveWorkspaceIndex, 0, (int)m_Workspaces.size() - 1);
    return m_Workspaces[(size_t)clampedIndex];
}

#pragma once

#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/EditorShortcuts.h"
#include "Editor/AssetEditors/AssetEditor.h"
#include "Editor/AssetEditors/AnimationAssetEditors.h"
#include "Editor/Panels/OutlinerPanel.h"
#include "Editor/Panels/DetailsPanel.h"
#include "Editor/Panels/GameModePanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/ContentBrowserPanel.h"
#include "Editor/Panels/PrefabEditorPanel.h"
#include "Editor/Panels/MenuBarPanel.h"
#include "Editor/Panels/LevelToolbarPanel.h"
#include "Editor/Panels/DebugPanel.h"
#include "Editor/Panels/StaticMeshImporterPanel.h"
#include "Editor/Panels/AnimationImporterPanel.h"
#include "Editor/Panels/AnimationDebuggerPanel.h"
#include "Editor/Panels/SkeletalMeshImporterPanel.h"
#include "imgui.h"

#include <array>

enum class EditorWindowResizeRegion
{
    None,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

enum class EditorDockLayer
{
    ShellWorkspace,
    GlobalUtilityTab,
    EditorScopedPanel
};

enum class EditorWorkspaceKind
{
    LevelEditor,
    BlueprintEditor,
    MaterialEditor,
    AnimationEditor
};

enum class EditorPanelKind
{
    Toolbar,
    Viewport,
    Outliner,
    Details
};

enum class GlobalUtilityTabKind
{
    ContentBrowser,
    Console,
    GameMode,
    AnimationDebugger,
    StaticMeshImporter,
    SkeletalMeshImporter,
    AnimationImporter
};

enum class DroppedImportKind
{
    StaticMesh,
    SkeletalMesh,
    Animation
};

struct DockableWindowDescriptor
{
    DockableWindowDescriptor() = default;
    DockableWindowDescriptor(EditorDockLayer layer, const char* windowName)
        : Layer(layer)
        , WindowName(windowName)
    {
    }

    EditorDockLayer Layer = EditorDockLayer::GlobalUtilityTab;
    const char* WindowName = "";
};

struct EditorPanelDescriptor : DockableWindowDescriptor
{
    EditorPanelDescriptor() = default;
    EditorPanelDescriptor(EditorDockLayer layer, const char* windowName, EditorPanelKind kind, bool toolbarStyle)
        : DockableWindowDescriptor(layer, windowName)
        , Kind(kind)
        , ToolbarStyle(toolbarStyle)
    {
    }

    EditorPanelKind Kind = EditorPanelKind::Viewport;
    bool ToolbarStyle = false;
};

struct GlobalUtilityTabDescriptor : DockableWindowDescriptor
{
    GlobalUtilityTabDescriptor() = default;
    GlobalUtilityTabDescriptor(EditorDockLayer layer, const char* windowName, GlobalUtilityTabKind kind, bool dockByDefault)
        : DockableWindowDescriptor(layer, windowName)
        , Kind(kind)
        , DockByDefault(dockByDefault)
    {
    }

    GlobalUtilityTabKind Kind = GlobalUtilityTabKind::ContentBrowser;
    bool DockByDefault = false;
};

struct EditorWorkspace
{
    EditorDockLayer Layer = EditorDockLayer::ShellWorkspace;
    EditorWorkspaceKind Kind = EditorWorkspaceKind::LevelEditor;
    const char* Label = "";
    const char* WorkspaceWindowName = "";
    const char* HostChildName = "";
    const char* DockspaceName = "";
    std::array<EditorPanelDescriptor, 4> Panels{};
    ImGuiID WorkspaceClassId = 0;
    ImGuiID DockspaceClassId = 0;
    bool LayoutInitialized = false;
};

class EditorLayer
{
public:
    EditorLayer();

    void TickShortcuts();
    void Draw(float dt);
    void ToggleContentBrowserVisibility();

private:
    void DrawRootShell();
    float DrawTitlebar();
    void HandleWindowResize(const ImVec2& windowMin, const ImVec2& windowMax);
    void DrawTitlebarMenu(const ImVec2& titlebarMin, float titlebarHeight, bool& menuHovered);
    void DrawTitlebarControls(const ImVec2& titlebarMin, float titlebarHeight, bool& controlsHovered);
    void DrawOuterDockspace();
    void DrawWorkspace(EditorWorkspace& workspace, float dt);
    void EnsureOuterLayout(ImGuiID documentDockId, const ImVec2& documentSize);
    void EnsureWorkspaceLayout(EditorWorkspace& workspace, ImGuiID dockspaceID, const ImVec2& size);
    void CollectDroppedFiles();
    void DrawGlobalUtilityWindows();
    void DrawDroppedImportPopup();
    EditorWorkspace& GetActiveWorkspace();
    const EditorWorkspace& GetActiveWorkspace() const;

private:
    EditorSelection& m_Selection;
    EditorShortcuts m_Shortcuts;
    MenuBarPanel m_MenuBar;
    LevelToolbarPanel m_LevelToolbar;
    OutlinerPanel m_Outliner;
    DetailsPanel m_Details;
    ViewportPanel m_Viewport;
    ContentBrowserPanel m_ContentBrowser;
    GameModePanel m_GameModePanel;
    PrefabEditorPanel m_PrefabEditor;
    AnimationAssetEditor m_AnimationAssetEditor;
    SkeletonAssetEditor m_SkeletonAssetEditor;
    SkeletalMeshAssetEditor m_SkeletalMeshAssetEditor;
    AnimGraphAssetEditor m_AnimGraphAssetEditor;
    AssetEditorManager m_AssetEditorManager;
    DebugPanel m_Debug;
    StaticMeshImporterPanel m_StaticMeshImporter;
    SkeletalMeshImporterPanel m_SkeletalMeshImporter;
    AnimationImporterPanel m_AnimationImporter;
    AnimationDebuggerPanel m_AnimationDebugger;
    std::array<EditorWorkspace, 1> m_Workspaces{};
    std::array<GlobalUtilityTabDescriptor, 7> m_GlobalUtilityTabs{};
    int m_ActiveWorkspaceIndex = 0;
    bool m_OuterLayoutInitialized = false;
    bool m_ShowContentBrowser = true;
    ImGuiID m_ShellDocumentDockId = 0;
    bool m_TitlebarHovered = false;
    bool m_WindowDragActive = false;
    bool m_WindowDragStartedMaximized = false;
    bool m_WindowResizeActive = false;
    ImVec2 m_WindowDragOffset = ImVec2(0.0f, 0.0f);
    ImVec2 m_WindowResizeStartMouse = ImVec2(0.0f, 0.0f);
    ImVec2 m_WindowResizeStartPos = ImVec2(0.0f, 0.0f);
    ImVec2 m_WindowResizeStartSize = ImVec2(0.0f, 0.0f);
    float m_WindowDragRestoreRatio = 0.5f;
    EditorWindowResizeRegion m_WindowResizeRegion = EditorWindowResizeRegion::None;
    bool m_ShowDroppedImportPopup = false;
    bool m_OpenDroppedImportPopupNextFrame = false;
    TArray<String> m_DroppedImportFiles;
    char m_DroppedImportOutputDirectory[512] = "assets";
    AssetHandle m_DroppedImportSkeletonHandle = 0;
    DroppedImportKind m_DroppedImportKind = DroppedImportKind::StaticMesh;
    String m_DroppedImportStatus;
};

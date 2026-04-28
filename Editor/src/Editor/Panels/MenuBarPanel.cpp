#include "Editor/Panels/MenuBarPanel.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/BaseAsset.h"

#include "EditorEngine.h"
#include "Editor/Core/EditorCommandSystem.h"
#include "Editor/Core/WindowsFileDialogs.h"

#include "imgui.h"
#include "ThirdParty/IconsFontAwesome6.h"

void MenuBarPanel::SetAnimationImporterVisibility(bool* showAnimationImporterWindow)
{
    m_ShowAnimationImporterWindow = showAnimationImporterWindow;
}

void MenuBarPanel::SetStaticMeshImporterVisibility(bool* showStaticMeshImporterWindow)
{
    m_ShowStaticMeshImporterWindow = showStaticMeshImporterWindow;
}

void MenuBarPanel::SetSkeletalMeshImporterVisibility(bool* showSkeletalMeshImporterWindow)
{
    m_ShowSkeletalMeshImporterWindow = showSkeletalMeshImporterWindow;
}

bool MenuBarPanel::HandleOpenScene()
{
    EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine);
    if (!editor)
        return false;

    const String path = Editor::WindowsFileDialogs::OpenFile(
        L"Open Scene",
        L"Rebel Scene Files (*.Ryml)\0*.Ryml\0All Files (*.*)\0*.*\0");
    if (path.length() == 0)
        return false;

    return editor->LoadEditorScene(path);
}

bool MenuBarPanel::HandleSaveScene(const bool saveAs)
{
    EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine);
    if (!editor)
        return false;

    String path;
    if (saveAs || editor->GetCurrentScenePath().length() == 0)
    {
        path = Editor::WindowsFileDialogs::SaveFile(
            L"Save Scene As",
            L"Rebel Scene Files (*.Ryml)\0*.Ryml\0All Files (*.*)\0*.*\0",
            L"Ryml");
        if (path.length() == 0)
            return false;
    }

    return editor->SaveEditorScene(path);
}

void MenuBarPanel::Draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.105f, 0.108f, 0.114f, 1.0f));

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem(ICON_FA_FILE " New");
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open..."))
                HandleOpenScene();
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save"))
                HandleSaveScene(false);
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As..."))
                HandleSaveScene(true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            EditorTransactionManager& tx = GetEditorTransactionManager();

            const bool canUndo = tx.CanUndo();
            const bool canRedo = tx.CanRedo();

            if (ImGui::MenuItem(ICON_FA_ARROW_ROTATE_LEFT " Undo", "Ctrl+Z", false, canUndo))
                tx.Undo();

            if (ImGui::MenuItem(ICON_FA_ARROW_ROTATE_RIGHT " Redo", "Ctrl+Y", false, canRedo))
                tx.Redo();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS " Create New Asset"))
                m_TestAsset = GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetManager().Create<Asset>();

            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Asset") && m_TestAsset)
                GEngine->GetModuleManager().GetModule<AssetManagerModule>()->SaveAssetHeader(*m_TestAsset);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Binary Assets"))
        {
            if (ImGui::MenuItem(ICON_FA_CUBE " Import Static Mesh") && m_ShowStaticMeshImporterWindow)
                *m_ShowStaticMeshImporterWindow = true;

            if (ImGui::MenuItem(ICON_FA_PERSON_RUNNING " Import Skeletal Mesh") && m_ShowSkeletalMeshImporterWindow)
                *m_ShowSkeletalMeshImporterWindow = true;

            if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import Animations") && m_ShowAnimationImporterWindow)
                *m_ShowAnimationImporterWindow = true;

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

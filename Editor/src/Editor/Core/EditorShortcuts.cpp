#include "Editor/Core/EditorShortcuts.h"

#include "EditorEngine.h"
#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"
#include "Editor/Core/EditorCommandSystem.h"
#include "Editor/Core/EditorContext.h"

#include "imgui.h"

namespace
{
void FocusSelectedActor()
{
    EditorContext& context = GetEditorContext();
    if (!context.Selection || !context.Selection->SelectedActor)
        return;

    Actor* actor = context.Selection->SelectedActor;
    if (!actor->IsValid())
        return;

    EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine);
    if (!editor || !editor->GetEditorCamera())
        return;

    const Vector3 target = actor->GetRootComponent()
        ? actor->GetRootComponent()->GetWorldPosition()
        : actor->GetActorLocation();
    editor->GetEditorCamera()->SetPosition(target + Vector3(-6.0f, -6.0f, 4.0f));
    editor->GetEditorCamera()->SetRotation(45.0f, -28.0f);
}
}

void EditorShortcuts::SetToggleContentBrowserAction(std::function<void()> action)
{
    m_ToggleContentBrowserAction = std::move(action);
}

void EditorShortcuts::Tick()
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.WantTextInput)
        return;

    EditorContext& context = GetEditorContext();

    if (ImGui::IsKeyPressed(ImGuiKey_F, false))
    {
        FocusSelectedActor();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && context.Selection && context.Selection->SelectedActor)
    {
        EditorCommandDispatcher::Execute(std::make_unique<DeleteActorCommand>(context.Selection->SelectedActor));
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && context.Selection && context.Selection->SelectedActor)
    {
        EditorCommandDispatcher::Execute(std::make_unique<DuplicateActorCommand>(context.Selection->SelectedActor));
        return;
    }

    if (!io.KeyCtrl)
        return;

    EditorTransactionManager& tx = GetEditorTransactionManager();

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (io.KeyShift)
            tx.Redo();
        else
            tx.Undo();

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
        tx.Redo();

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && m_ToggleContentBrowserAction)
    {
        m_ToggleContentBrowserAction();
        return;
    }
}

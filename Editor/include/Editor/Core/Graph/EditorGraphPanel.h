#pragma once

#include "Editor/Core/Graph/EditorGraphTypes.h"

namespace EditorGraphPanel
{
    GraphEditor::Options MakePoseGraphOptions();
    GraphEditor::Options MakeStateMachineGraphOptions();

    ImVec2 ScreenToGraphPosition(
        const ImVec2& screenPosition,
        const ImVec2& canvasScreenPosition,
        const GraphEditor::ViewState& viewState);

    EditorGraphContextEvent Show(
        GraphEditor::Delegate& delegate,
        const GraphEditor::Options& options,
        EditorGraphDocumentState& documentState,
        bool enabled = true);

    void ClearInteractionState();
}

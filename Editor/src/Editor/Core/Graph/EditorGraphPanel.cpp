#include "Editor/Core/Graph/EditorGraphPanel.h"

namespace EditorGraphPanel
{
    GraphEditor::Options MakePoseGraphOptions()
    {
        GraphEditor::Options options;
        options.mBackgroundColor = IM_COL32(17, 19, 23, 255);
        options.mGridColor = IM_COL32(255, 255, 255, 22);
        options.mGridColor2 = IM_COL32(255, 255, 255, 42);
        options.mSelectedNodeBorderColor = IM_COL32(230, 170, 70, 255);
        options.mNodeBorderColor = IM_COL32(76, 82, 94, 120);
        options.mFrameFocus = IM_COL32(230, 170, 70, 255);
        options.mDefaultSlotColor = IM_COL32(105, 178, 255, 255);
        options.mRounding = 5.0f;
        options.mDisplayLinksAsCurves = true;
        return options;
    }

    GraphEditor::Options MakeStateMachineGraphOptions()
    {
        GraphEditor::Options options;
        options.mBackgroundColor = IM_COL32(17, 19, 23, 255);
        options.mGridColor = IM_COL32(255, 255, 255, 22);
        options.mGridColor2 = IM_COL32(255, 255, 255, 42);
        options.mSelectedNodeBorderColor = IM_COL32(100, 205, 150, 255);
        options.mNodeBorderColor = IM_COL32(76, 82, 94, 120);
        options.mFrameFocus = IM_COL32(100, 205, 150, 255);
        options.mDefaultSlotColor = IM_COL32(100, 205, 150, 255);
        options.mRounding = 5.0f;
        options.mDisplayLinksAsCurves = true;
        return options;
    }

    ImVec2 ScreenToGraphPosition(
        const ImVec2& screenPosition,
        const ImVec2& canvasScreenPosition,
        const GraphEditor::ViewState& viewState)
    {
        return ImVec2(
            (screenPosition.x - canvasScreenPosition.x) / viewState.mFactor - viewState.mPosition.x,
            (screenPosition.y - canvasScreenPosition.y) / viewState.mFactor - viewState.mPosition.y);
    }

    EditorGraphContextEvent Show(
        GraphEditor::Delegate& delegate,
        const GraphEditor::Options& options,
        EditorGraphDocumentState& documentState,
        const bool enabled)
    {
        GraphEditor::Show(delegate, options, documentState.ViewState, enabled, &documentState.Fit);
        return {};
    }

    void ClearInteractionState()
    {
        GraphEditor::GraphEditorClear();
    }
}

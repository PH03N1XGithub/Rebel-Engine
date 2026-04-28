#pragma once

#include "Core/CoreTypes.h"
#include "GraphEditor.h"
#include "imgui.h"

enum class EditorGraphContextTarget
{
    None,
    Canvas,
    Node,
    Link
};

struct EditorGraphContextEvent
{
    EditorGraphContextTarget Target = EditorGraphContextTarget::None;
    ImVec2 GraphPosition = ImVec2(0.0f, 0.0f);
    uint64 NodeID = 0;
    uint64 LinkID = 0;

    bool IsValid() const { return Target != EditorGraphContextTarget::None; }
};

struct EditorGraphDocumentState
{
    GraphEditor::ViewState ViewState;
    GraphEditor::FitOnScreen Fit = GraphEditor::Fit_None;
    uint64 SelectedNodeID = 0;
    ImVec2 ContextGraphPosition = ImVec2(0.0f, 0.0f);
};

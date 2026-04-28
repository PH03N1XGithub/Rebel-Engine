#pragma once

#include "Editor/Core/EditorCommands.h"
#include "Editor/Core/EditorSelection.h"

struct ImVec2;

enum class GizmoOperation
{
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace
{
    World,
    Local
};

class GizmoSystem
{
public:
    explicit GizmoSystem(EditorSelection& selection);

    void SetViewportHovered(bool hovered);
    void SetOperation(GizmoOperation operation) { m_Operation = operation; }
    GizmoOperation GetOperation() const { return m_Operation; }
    void SetMode(GizmoSpace mode) { m_Mode = mode; }
    GizmoSpace GetMode() const { return m_Mode; }
    void Draw(const ImVec2& viewportPos, const ImVec2& viewportSize);

    bool IsUsing() const { return m_IsUsing; }

private:
    EditorSelection& m_Selection;
    bool m_ViewportHovered = false;
    bool m_IsUsing = false;
    bool m_WasUsingLastFrame = false;
    bool m_HasBeginSnapshot = false;
    GizmoOperation m_Operation = GizmoOperation::Translate;
    GizmoSpace m_Mode = GizmoSpace::World;
    LocalTransformSnapshot m_DragBeginSnapshot;
};

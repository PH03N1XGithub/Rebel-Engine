#pragma once

#include "Editor/Gizmo/GizmoSystem.h"
#include "Editor/Core/EditorSelection.h"

#include "imgui.h"
#include <ThirdParty/entt.h>

class RenderModule;

class ViewportPanel
{
public:
    explicit ViewportPanel(EditorSelection& selection);

    void SetDeltaTime(float dt);
    void Draw();

private:
    bool HandleViewportPicking(
        RenderModule* renderer,
        const ImVec2& imagePos,
        const ImVec2& viewportSize,
        entt::entity& outHitEntity) const;
    bool DrawViewportTools(const ImVec2& imagePos, int fps);
    void DrawViewportStatus(const ImVec2& imagePos, const ImVec2& imageMax, const ImVec2& viewportSize, int fps);

private:
    EditorSelection& m_Selection;
    GizmoSystem m_Gizmo;
    bool m_Inside = false;
    float m_DeltaTime = 0.0f;
};

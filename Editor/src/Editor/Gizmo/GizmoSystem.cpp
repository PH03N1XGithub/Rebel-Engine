#include "Editor/Gizmo/GizmoSystem.h"

#include "Editor/Core/EditorCommandDispatcher.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Rendering/Camera.h"
#include "Engine/Components/SceneComponent.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace
{
ImGuizmo::OPERATION ToImGuizmoOperation(GizmoOperation operation)
{
    switch (operation)
    {
    case GizmoOperation::Rotate:
        return ImGuizmo::ROTATE;
    case GizmoOperation::Scale:
        return ImGuizmo::SCALE;
    case GizmoOperation::Translate:
    default:
        return ImGuizmo::TRANSLATE;
    }
}

ImGuizmo::MODE ToImGuizmoMode(GizmoSpace space)
{
    return (space == GizmoSpace::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

bool HasTransformDelta(const LocalTransformSnapshot& lhs, const LocalTransformSnapshot& rhs)
{
    constexpr float kPosEpsilon = 1e-4f;
    constexpr float kScaleEpsilon = 1e-4f;
    constexpr float kRotDotEpsilon = 1e-5f;

    if (glm::length(rhs.Position - lhs.Position) > kPosEpsilon)
        return true;

    if (glm::length(rhs.Scale - lhs.Scale) > kScaleEpsilon)
        return true;

    const float absDot = std::fabs(glm::dot(glm::normalize(lhs.Rotation), glm::normalize(rhs.Rotation)));
    return (1.0f - absDot) > kRotDotEpsilon;
}

SceneComponent* ResolveGizmoTarget(EditorSelection& selection)
{
    if (selection.SelectedComponent)
    {
        if (auto* selectedSceneComponent = dynamic_cast<SceneComponent*>(selection.SelectedComponent))
        {
            if (selectedSceneComponent->GetOwner() && selectedSceneComponent->GetOwner()->IsValid())
                return selectedSceneComponent;
        }
    }

    Actor* selected = selection.SelectedActor;
    if (!selected || !selected->IsValid() || !selected->HasComponent<SceneComponent>())
        return nullptr;

    return &selected->GetComponent<SceneComponent>();
}
}

GizmoSystem::GizmoSystem(EditorSelection& selection)
    : m_Selection(selection)
{
}

void GizmoSystem::SetViewportHovered(bool hovered)
{
    m_ViewportHovered = hovered;
}

void GizmoSystem::Draw(const ImVec2& viewportPos, const ImVec2& viewportSize)
{
    SceneComponent* sceneComponent = ResolveGizmoTarget(m_Selection);
    if (!sceneComponent)
    {
        m_IsUsing = false;
        m_WasUsingLastFrame = false;
        m_HasBeginSnapshot = false;
        return;
    }

    const float safeHeight = viewportSize.y > 1.0f ? viewportSize.y : 1.0f;
    const float aspect = viewportSize.x / safeHeight;

    CameraView cam = GEngine->GetActiveCamera(aspect);
    glm::mat4 view = cam.View;
    glm::mat4 proj = cam.Projection;

    SceneComponent* parent = sceneComponent->GetParent();
    const glm::mat4 parentWorld = parent ? parent->GetWorldTransform() : glm::mat4(1.0f);
    glm::mat4 model = parentWorld * sceneComponent->GetLocalTransform();

    ImGuizmo::SetOrthographic(false);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRectFullScreen();
    ImGuizmo::SetDrawlist(drawList);
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    if (m_ViewportHovered)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_Operation = GizmoOperation::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_Operation = GizmoOperation::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_Operation = GizmoOperation::Scale;
    }

    const bool snap = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    float snapVals[3] = { 0.5f, 0.5f, 0.5f };
    if (m_Operation == GizmoOperation::Rotate)
        snapVals[0] = snapVals[1] = snapVals[2] = 15.0f;

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        ToImGuizmoOperation(m_Operation),
        ToImGuizmoMode(m_Mode),
        glm::value_ptr(model),
        nullptr,
        snap ? snapVals : nullptr);

    drawList->PopClipRect();

    m_IsUsing = ImGuizmo::IsUsing();
    if (m_IsUsing && !m_WasUsingLastFrame)
    {
        m_DragBeginSnapshot = TransformSceneComponentCommand::Capture(*sceneComponent);
        m_HasBeginSnapshot = true;
        EditorCommandDispatcher::BeginTransaction("Transform Component");
    }

    if (m_IsUsing)
    {
        glm::mat4 newWorld = model;
        glm::mat4 newLocal = glm::inverse(parentWorld) * newWorld;

        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        glm::decompose(newLocal, scale, rotation, translation, skew, perspective);

        sceneComponent->SetPosition({ translation.x, translation.y, translation.z });
        sceneComponent->SetScale({ scale.x, scale.y, scale.z });
        sceneComponent->SetRotationQuat(rotation);
    }
    else if (m_WasUsingLastFrame && m_HasBeginSnapshot)
    {
        const LocalTransformSnapshot dragEndSnapshot = TransformSceneComponentCommand::Capture(*sceneComponent);
        if (HasTransformDelta(m_DragBeginSnapshot, dragEndSnapshot))
        {
            EditorCommandDispatcher::Execute(
                std::make_unique<TransformSceneComponentCommand>(
                    sceneComponent,
                    m_DragBeginSnapshot,
                    dragEndSnapshot,
                    "Transform Component"));
            EditorCommandDispatcher::CommitTransaction();
        }
        else
        {
            EditorCommandDispatcher::CancelTransaction();
        }

        m_HasBeginSnapshot = false;
    }

    m_WasUsingLastFrame = m_IsUsing;
}

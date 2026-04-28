#include "Editor/Core/EditorSelection.h"
#include "Engine/Framework/EnginePch.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/Scene.h"

#include <algorithm>

namespace
{
bool IsSelectableActor(const Actor* actor)
{
    return actor && actor->IsValid() && actor->GetHandle() != entt::null;
}

void ClearComponentSelection(EditorSelection& selection)
{
    selection.SelectedComponent = nullptr;
    selection.SelectedComponentType = nullptr;
}
}

void EditorSelection::Clear()
{
    SelectedActor = nullptr;
    ClearComponentSelection(*this);
    m_SelectedActorHandles.clear();
    m_PrimaryActorHandle = entt::null;
}

void EditorSelection::SetSingleActor(Actor* actor)
{
    Clear();

    if (!IsSelectableActor(actor))
        return;

    SelectedActor = actor;
    m_SelectedActorHandles.push_back(actor->GetHandle());
    m_PrimaryActorHandle = actor->GetHandle();
}

void EditorSelection::AddActor(Actor* actor)
{
    if (!IsSelectableActor(actor))
        return;

    const entt::entity handle = actor->GetHandle();
    if (!IsActorHandleSelected(handle))
        m_SelectedActorHandles.push_back(handle);

    SelectedActor = actor;
    m_PrimaryActorHandle = handle;
    ClearComponentSelection(*this);
}

void EditorSelection::ToggleActor(Actor* actor)
{
    if (!IsSelectableActor(actor))
        return;

    if (IsActorSelected(actor))
    {
        RemoveActor(actor);
        return;
    }

    AddActor(actor);
}

void EditorSelection::RemoveActor(Actor* actor)
{
    if (!IsSelectableActor(actor))
        return;

    const entt::entity handle = actor->GetHandle();
    m_SelectedActorHandles.erase(
        std::remove(m_SelectedActorHandles.begin(), m_SelectedActorHandles.end(), handle),
        m_SelectedActorHandles.end());

    if (m_PrimaryActorHandle == handle)
        m_PrimaryActorHandle = entt::null;

    if (m_SelectedActorHandles.empty())
    {
        SelectedActor = nullptr;
        m_PrimaryActorHandle = entt::null;
        ClearComponentSelection(*this);
        return;
    }

    Scene* scene = actor->GetScene();
    if (!scene)
    {
        SelectedActor = nullptr;
        return;
    }

    if (m_PrimaryActorHandle == entt::null || !IsActorHandleSelected(m_PrimaryActorHandle))
        m_PrimaryActorHandle = m_SelectedActorHandles.back();

    SelectedActor = scene->GetActor(m_PrimaryActorHandle);
}

bool EditorSelection::IsActorSelected(const Actor* actor) const
{
    return IsSelectableActor(actor) && IsActorHandleSelected(actor->GetHandle());
}

bool EditorSelection::IsActorHandleSelected(entt::entity handle) const
{
    return std::find(m_SelectedActorHandles.begin(), m_SelectedActorHandles.end(), handle) != m_SelectedActorHandles.end();
}

size_t EditorSelection::GetSelectedActorCount() const
{
    return m_SelectedActorHandles.size();
}

const std::vector<entt::entity>& EditorSelection::GetSelectedActorHandles() const
{
    return m_SelectedActorHandles;
}

void EditorSelection::SyncWithScene(Scene* scene)
{
    if (!scene)
    {
        Clear();
        return;
    }

    m_SelectedActorHandles.erase(
        std::remove_if(
            m_SelectedActorHandles.begin(),
            m_SelectedActorHandles.end(),
            [scene](entt::entity handle)
            {
                Actor* actor = scene->GetActor(handle);
                return !IsSelectableActor(actor);
            }),
        m_SelectedActorHandles.end());

    if (m_PrimaryActorHandle == entt::null || !IsActorHandleSelected(m_PrimaryActorHandle))
        m_PrimaryActorHandle = m_SelectedActorHandles.empty() ? entt::null : m_SelectedActorHandles.back();

    SelectedActor = (m_PrimaryActorHandle != entt::null) ? scene->GetActor(m_PrimaryActorHandle) : nullptr;
    if (!SelectedActor && !m_SelectedActorHandles.empty())
    {
        m_PrimaryActorHandle = m_SelectedActorHandles.back();
        SelectedActor = scene->GetActor(m_PrimaryActorHandle);
    }

    if (m_SelectedActorHandles.empty())
    {
        m_PrimaryActorHandle = entt::null;
        ClearComponentSelection(*this);
        return;
    }

    if (SelectedComponent && SelectedComponent->GetOwner() != SelectedActor)
    {
        ClearComponentSelection(*this);
    }
}

EditorSelection& GetEditorSelection()
{
    static EditorSelection selection;
    return selection;
}

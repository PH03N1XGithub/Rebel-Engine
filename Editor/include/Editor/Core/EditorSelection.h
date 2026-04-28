#pragma once

#include <ThirdParty/entt.h>
#include <vector>

class Actor;
class Scene;
struct EntityComponent;

namespace Rebel::Core::Reflection
{
    struct TypeInfo;
}

struct EditorSelection
{
    Actor* SelectedActor = nullptr;
    EntityComponent* SelectedComponent = nullptr;
    const Rebel::Core::Reflection::TypeInfo* SelectedComponentType = nullptr;

    void Clear();
    void SetSingleActor(Actor* actor);
    void AddActor(Actor* actor);
    void ToggleActor(Actor* actor);
    void RemoveActor(Actor* actor);

    bool IsActorSelected(const Actor* actor) const;
    bool IsActorHandleSelected(entt::entity handle) const;

    size_t GetSelectedActorCount() const;
    const std::vector<entt::entity>& GetSelectedActorHandles() const;

    void SyncWithScene(Scene* scene);

private:
    std::vector<entt::entity> m_SelectedActorHandles;
    entt::entity m_PrimaryActorHandle = entt::null;
};

EditorSelection& GetEditorSelection();

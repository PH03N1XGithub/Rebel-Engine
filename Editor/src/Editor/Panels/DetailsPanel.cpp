#include "Editor/Panels/DetailsPanel.h"

#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "imgui.h"

#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/Scene.h"
#include "ThirdParty/IconsFontAwesome6.h"

DEFINE_LOG_CATEGORY(EditorDetailsLog)

namespace
{
bool IsCoreComponent(const Rebel::Core::Reflection::ComponentTypeInfo& info)
{
    return info.Name == "SceneComponent" ||
        info.Name == "IDComponent" ||
        info.Name == "NameComponent";
}

const Rebel::Core::Reflection::ComponentTypeInfo* FindComponentInfoForType(const Rebel::Core::Reflection::TypeInfo* type)
{
    if (!type)
        return nullptr;

    for (const auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (info.Type == type)
            return &info;
    }

    return nullptr;
}

bool IsObjectComponentType(const Rebel::Core::Reflection::ComponentTypeInfo& info)
{
    return info.Type && info.Type->IsA(EntityComponent::StaticType());
}

void DrawComponentContextMenu(
    Actor& actor,
    const Rebel::Core::Reflection::ComponentTypeInfo& info,
    EditorSelection& selection,
    EntityComponent* componentInstance)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    const bool canDuplicate = componentInstance && IsObjectComponentType(info) && !IsCoreComponent(info);
    if (ImGui::MenuItem(ICON_FA_COPY " Duplicate Component", nullptr, false, canDuplicate))
        EditorCommandDispatcher::Execute(std::make_unique<DuplicateComponentCommand>(&actor, &info, componentInstance));
    if (!canDuplicate && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Only actor-owned object components can be duplicated; data components stay type-unique.");

    const bool canRemove = !IsCoreComponent(info) && info.RemoveFn && info.HasFn && info.HasFn(actor);
    if (ImGui::MenuItem(ICON_FA_TRASH " Remove Component", nullptr, false, canRemove))
    {
        EditorCommandDispatcher::Execute(std::make_unique<RemoveComponentCommand>(&actor, &info, componentInstance));
        selection.SelectedComponent = nullptr;
        selection.SelectedComponentType = nullptr;
    }

    ImGui::EndPopup();
}

void DrawComponentHierarchy(entt::registry& reg, entt::entity e, Actor& actor, EditorSelection& selection)
{
    (void)reg;
    (void)e;

    ImGuiTreeNodeFlags rootFlags =
          ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    SceneComponent* rootComponent = actor.GetRootComponent();
    if (selection.SelectedComponent == rootComponent)
        rootFlags |= ImGuiTreeNodeFlags_Selected;

    bool rootOpen = ImGui::TreeNodeEx("Scene Root", rootFlags, "%s Scene Root", ICON_FA_CIRCLE_NODES);

    bool rootClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool rootDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();

    if (rootClicked && !rootDouble)
    {
        selection.SelectedComponent = rootComponent;
        selection.SelectedComponentType = rootComponent ? rootComponent->GetType() : nullptr;
    }

    if (rootOpen)
    {
        for (auto& info : ComponentRegistry::Get().GetComponents())
        {
            if (!info.HasFn || !info.GetFn)
                continue;
            if (IsObjectComponentType(info))
                continue;
            if (!info.HasFn(actor))
                continue;
            if (IsCoreComponent(info))
                continue;

            ImGuiTreeNodeFlags flags =
                  ImGuiTreeNodeFlags_Leaf
                | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::TreeNodeEx(info.Name.c_str(), flags, "%s %s", ICON_FA_PUZZLE_PIECE, info.Name.c_str());

            bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool dbl = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();

            if (clicked && !dbl)
            {
                selection.SelectedComponent = nullptr;
                selection.SelectedComponentType = info.Type;
            }

            DrawComponentContextMenu(actor, info, selection, nullptr);
        }

        int objectComponentIndex = 0;
        for (const auto& componentPtr : actor.GetObjectComponents())
        {
            EntityComponent* component = componentPtr.Get();
            if (!component)
                continue;

            const auto* info = FindComponentInfoForType(component->GetType());
            if (!info || IsCoreComponent(*info))
                continue;

            ImGuiTreeNodeFlags flags =
                  ImGuiTreeNodeFlags_Leaf
                | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (selection.SelectedComponent == component)
                flags |= ImGuiTreeNodeFlags_Selected;

            const String displayName = component->GetEditorName().length() > 0 ? component->GetEditorName() : info->Name;
            const String label = info->Name + "##ObjectComponent" + String(std::to_string(objectComponentIndex++).c_str());
            ImGui::TreeNodeEx(label.c_str(), flags, "%s %s", ICON_FA_PUZZLE_PIECE, displayName.c_str());

            bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool dbl = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();

            if (clicked && !dbl)
            {
                selection.SelectedComponent = component;
                selection.SelectedComponentType = info->Type;
            }

            DrawComponentContextMenu(actor, *info, selection, component);
        }

        ImGui::TreePop();
    }
}
}

DetailsPanel::DetailsPanel(EditorSelection& selection)
    : m_Selection(selection)
{
}

void DetailsPanel::Draw()
{
    ImGui::SeparatorText(ICON_FA_SLIDERS " Details");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    if (Scene* activeScene = GEngine->GetActiveScene())
        m_Selection.SyncWithScene(activeScene);

    Actor* selected = m_Selection.SelectedActor;
    if (!selected || !selected->IsValid())
    {
        ImGui::TextDisabled("No Actor selected.");
        return;
    }

    if (selected->HasComponent<NameComponent>())
    {
        auto& nameComp = selected->GetComponent<NameComponent>();

        char buffer[256];
        strncpy_s(buffer, nameComp.Name.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        const String beforeName = nameComp.Name;
        ImGui::TextDisabled("Actor Name");
        const bool changed = ImGui::InputText("##ActorName", buffer, sizeof(buffer));
        if (ImGui::IsItemActivated())
            EditorCommandDispatcher::BeginTransaction("Rename Actor");

        if (changed)
        {
            EditorCommandDispatcher::Execute(std::make_unique<RenameActorCommand>(
                selected,
                beforeName,
                String(buffer)));
        }

        if (ImGui::IsItemDeactivatedAfterEdit())
            EditorCommandDispatcher::CommitTransaction();
    }

    if (ImGui::BeginPopupContextWindow("DetailsActorContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem(ICON_FA_COPY " Duplicate Actor"))
            EditorCommandDispatcher::Execute(std::make_unique<DuplicateActorCommand>(selected));

        if (ImGui::MenuItem(ICON_FA_TRASH " Delete Actor"))
            EditorCommandDispatcher::Execute(std::make_unique<DeleteActorCommand>(selected));

        ImGui::EndPopup();
    }

    ImGui::Separator();

    ImGui::SeparatorText(ICON_FA_CUBES_STACKED " Component Hierarchy");
    DrawComponentHierarchy(GEngine->GetActiveScene()->GetRegistry(), selected->GetHandle(), *selected, m_Selection);

    if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Add Component").c_str(), ImVec2(-FLT_MIN, 0.0f)))
        ImGui::OpenPopup("AddComponentPopup");

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        for (auto& info : ComponentRegistry::Get().GetComponents())
        {
            if (info.Name == "SceneComponent" || info.Name == "TagComponent" || info.Name == "ActorTagComponent")
                continue;

            bool has = !IsObjectComponentType(info) && info.HasFn && info.HasFn(*selected);

            if (has)
            {
                ImGui::BeginDisabled();
                ImGui::MenuItem(info.Name.c_str());
                ImGui::EndDisabled();
            }
            else if (ImGui::MenuItem(info.Name.c_str()))
            {
                EditorCommandDispatcher::Execute(std::make_unique<AddComponentCommand>(
                    selected,
                    &info));
                RB_LOG(EditorDetailsLog, info, "Added component %s", info.Name.c_str());
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SeparatorText(ICON_FA_GEARS " Properties");
    m_PropertyEditor.DrawComponentsForActor(*selected, m_Selection.SelectedComponentType, m_Selection.SelectedComponent);
}

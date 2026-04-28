#include "Editor/Panels/OutlinerPanel.h"

#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"
#include "Editor/UI/EditorImGui.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Scene/Scene.h"

#include "imgui.h"
#include "ThirdParty/IconsFontAwesome6.h"

namespace
{
Actor* GetParentActor(Actor& actor)
{
    SceneComponent* root = actor.GetRootComponent();
    if (!root)
        return nullptr;

    SceneComponent* parentRoot = root->GetParent();
    if (!parentRoot)
        return nullptr;

    Actor* parentActor = parentRoot->GetOwner();
    return (parentActor != &actor) ? parentActor : nullptr;
}

bool HasActorChildren(Scene& scene, Actor& actor)
{
    for (const auto& actorPtr : scene.GetActors())
    {
        Actor* candidate = actorPtr.Get();
        if (!candidate || !candidate->IsValid() || candidate == &actor)
            continue;

        if (GetParentActor(*candidate) == &actor)
            return true;
    }

    return false;
}

bool ContainsInsensitive(const String& text, const String& filterLower)
{
    if (filterLower.length() == 0)
        return true;

    const String lowerText = Rebel::Core::ToLower(text);
    if (filterLower.length() > lowerText.length())
        return false;

    for (size_t start = 0; start + filterLower.length() <= lowerText.length(); ++start)
    {
        bool matches = true;
        for (size_t i = 0; i < filterLower.length(); ++i)
        {
            if (lowerText[start + i] != filterLower[i])
            {
                matches = false;
                break;
            }
        }

        if (matches)
            return true;
    }

    return false;
}

bool ActorMatchesFilter(Actor& actor, const String& filterLower)
{
    if (filterLower.length() == 0)
        return true;

    if (ContainsInsensitive(actor.GetName(), filterLower))
        return true;

    const Rebel::Core::Reflection::TypeInfo* type = actor.GetType();
    return type && ContainsInsensitive(type->Name, filterLower);
}

bool SubtreeMatchesFilter(Scene& scene, Actor& actor, const String& filterLower)
{
    if (filterLower.length() == 0 || ActorMatchesFilter(actor, filterLower))
        return true;

    for (const auto& actorPtr : scene.GetActors())
    {
        Actor* child = actorPtr.Get();
        if (!child || !child->IsValid())
            continue;

        if (GetParentActor(*child) == &actor && SubtreeMatchesFilter(scene, *child, filterLower))
            return true;
    }

    return false;
}

void HandleOutlinerSelection(EditorSelection& selection, Actor& actor)
{
    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left))
        return;

    if (ImGui::GetIO().KeyCtrl)
        selection.ToggleActor(&actor);
    else
        selection.SetSingleActor(&actor);
}

void HandleOutlinerDragSource(Actor& actor)
{
    if (!ImGui::BeginDragDropSource())
        return;

    const entt::entity actorHandle = actor.GetHandle();
    ImGui::SetDragDropPayload("OUTLINER_ACTOR", &actorHandle, sizeof(actorHandle));
    ImGui::Text("%s %s", ICON_FA_CUBE, actor.GetName().c_str());
    ImGui::EndDragDropSource();
}

void HandleOutlinerDropTarget(Scene& scene, Actor& targetActor)
{
    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR"))
    {
        if (payload->DataSize == sizeof(entt::entity))
        {
            const entt::entity droppedHandle = *reinterpret_cast<const entt::entity*>(payload->Data);
            Actor* childActor = scene.GetActor(droppedHandle);

            if (childActor && childActor != &targetActor)
            {
                SceneComponent* childRoot = childActor->GetRootComponent();
                SceneComponent* targetRoot = targetActor.GetRootComponent();

                if (childRoot && targetRoot && childRoot->GetParent() != targetRoot && !targetRoot->IsDescendantOf(childRoot))
                {
                    EditorCommandDispatcher::Execute(
                        std::make_unique<ReparentActorCommand>(childActor, &targetActor));
                }
            }
        }
    }

    ImGui::EndDragDropTarget();
}

void HandleOutlinerContextMenu(Actor& actor)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    if (ImGui::MenuItem(ICON_FA_COPY " Duplicate"))
        EditorCommandDispatcher::Execute(std::make_unique<DuplicateActorCommand>(&actor));

    if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
        EditorCommandDispatcher::Execute(std::make_unique<DeleteActorCommand>(&actor));

    ImGui::EndPopup();
}

bool DrawActorNodeRecursive(Scene& scene, Actor& actor, EditorSelection& selection, const String& filterLower)
{
    auto& registry = scene.GetRegistry();
    const entt::entity actorEntity = actor.GetHandle();
    if (!registry.valid(actorEntity) || !registry.all_of<NameComponent>(actorEntity))
        return false;

    if (!SubtreeMatchesFilter(scene, actor, filterLower))
        return false;

    const String& name = registry.get<NameComponent>(actorEntity).Name;
    const bool selected = selection.IsActorSelected(&actor);
    const bool hasChildren = HasActorChildren(scene, actor);
    const bool showTypeLabel = selected || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGuiTreeNodeFlags flags =
        (selected ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (filterLower.length() > 0 && hasChildren)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    const char* icon = hasChildren ? ICON_FA_CUBES_STACKED : ICON_FA_CUBE;
    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<intptr_t>(actorEntity)),
        flags,
        "%s %s",
        icon,
        name.c_str());

    const bool hovered = ImGui::IsItemHovered();

    if (showTypeLabel && (hovered || selected))
    {
        const Rebel::Core::Reflection::TypeInfo* type = actor.GetType();
        if (type)
        {
            const char* typeLabel = type->Name.c_str();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImFont* smallFont = Rebel::Editor::UI::GetFont("Small");
            const ImVec2 typeSize = smallFont ? smallFont->CalcTextSizeA(smallFont->LegacySize, FLT_MAX, 0.0f, typeLabel) : ImGui::CalcTextSize(typeLabel);
            const float rightEdge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            const float textX = rightEdge - typeSize.x - 10.0f;

            if (textX > ImGui::GetItemRectMax().x + 10.0f)
            {
                drawList->AddText(
                    smallFont ? smallFont : ImGui::GetFont(),
                    smallFont ? smallFont->LegacySize : ImGui::GetFontSize(),
                    ImVec2(textX, ImGui::GetItemRectMin().y + 2.0f),
                    IM_COL32(132, 141, 156, 255),
                    typeLabel);
            }
        }
    }

    HandleOutlinerSelection(selection, actor);
    HandleOutlinerDragSource(actor);
    HandleOutlinerDropTarget(scene, actor);
    HandleOutlinerContextMenu(actor);

    if (!open)
        return true;

    if (hasChildren)
    {
        for (const auto& actorPtr : scene.GetActors())
        {
            Actor* child = actorPtr.Get();
            if (!child || !child->IsValid())
                continue;

            if (GetParentActor(*child) == &actor)
                DrawActorNodeRecursive(scene, *child, selection, filterLower);
        }
    }

    ImGui::TreePop();
    return true;
}
}

OutlinerPanel::OutlinerPanel(EditorSelection& selection)
    : m_Selection(selection)
{
}

void OutlinerPanel::Draw()
{
    using namespace Rebel::Editor::UI;

    Scene* scene = GEngine->GetActiveScene();
    if (scene)
        m_Selection.SyncWithScene(scene);

    {
        ScopedFont titleFont(GetFont("Bold"));
        ImGui::TextUnformatted("Scene Hierarchy");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s  %zu selected", ICON_FA_ARROW_POINTER, m_Selection.GetSelectedActorCount());

    {
        ScopedFont subFont(GetFont("Small"));
        ImGui::TextDisabled("Search actors, multi-select with Ctrl, and drag to reparent.");
    }
    DrawSearchField("##OutlinerSearch", m_FilterBuffer, IM_ARRAYSIZE(m_FilterBuffer), "Filter actors or types...");
    ImGui::Separator();

    if (!scene)
    {
        DrawEmptyState(ICON_FA_TRIANGLE_EXCLAMATION, "No active scene", "Open or create a scene to populate the hierarchy.");
        return;
    }

    const String filterLower = Rebel::Core::ToLower(String(m_FilterBuffer));
    int visibleRootCount = 0;

    for (const auto& actorPtr : scene->GetActors())
    {
        Actor* actor = actorPtr.Get();
        if (!actor || !actor->IsValid())
            continue;

        if (GetParentActor(*actor) == nullptr && DrawActorNodeRecursive(*scene, *actor, m_Selection, filterLower))
            ++visibleRootCount;
    }

    if (visibleRootCount == 0)
    {
        if (filterLower.length() > 0)
            DrawEmptyState(ICON_FA_FILTER, "No matching actors", "The current filter does not match any actor names or reflected actor types.");
        else
            DrawEmptyState(ICON_FA_INBOX, "Scene is empty", "Spawn an actor from the toolbar or drag one into the world to begin building the scene.");
    }

    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 8.0f));
    ImGui::SeparatorText(ICON_FA_ARROW_UP_FROM_BRACKET " Drop Here To Unparent");

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR"))
        {
            if (payload->DataSize == sizeof(entt::entity))
            {
                const entt::entity droppedHandle = *reinterpret_cast<const entt::entity*>(payload->Data);
                if (Actor* childActor = scene->GetActor(droppedHandle))
                {
                    SceneComponent* childRoot = childActor->GetRootComponent();
                    if (childRoot && childRoot->GetParent())
                    {
                        EditorCommandDispatcher::Execute(
                            std::make_unique<ReparentActorCommand>(childActor, nullptr));
                    }
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsWindowHovered() &&
        !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().KeyCtrl)
    {
        m_Selection.Clear();
    }
}
